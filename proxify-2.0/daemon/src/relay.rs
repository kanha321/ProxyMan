use common::{ProxyConfig, ConnectionInfo};
use std::sync::Arc;
use std::sync::atomic::{AtomicU64, Ordering};
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::sync::RwLock;

pub struct RelayServer {
    config: ProxyConfig,
    active_connections: Arc<RwLock<Vec<ConnectionInfo>>>,
    total_bytes_sent: Arc<AtomicU64>,
    total_bytes_recv: Arc<AtomicU64>,
}

impl RelayServer {
    pub fn new(
        config: ProxyConfig,
        active_connections: Arc<RwLock<Vec<ConnectionInfo>>>,
        total_bytes_sent: Arc<AtomicU64>,
        total_bytes_recv: Arc<AtomicU64>,
    ) -> Self {
        Self {
            config,
            active_connections,
            total_bytes_sent,
            total_bytes_recv,
        }
    }

    pub async fn run(&self) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
        let addr = format!("127.0.0.1:{}", self.config.relay_port);
        let listener = TcpListener::bind(&addr).await?;
        println!("[Relay] Listening on {} (Upstream {}:{})", addr, self.config.proxy_ip, self.config.proxy_port);

        loop {
            let (stream, peer_addr) = listener.accept().await?;
            let cfg = self.config.clone();
            let active_conns = self.active_connections.clone();
            let total_sent = self.total_bytes_sent.clone();
            let total_recv = self.total_bytes_recv.clone();

            tokio::spawn(async move {
                let client_port = peer_addr.port();
                if let Err(e) = Self::handle_connection(stream, client_port, cfg, active_conns, total_sent, total_recv).await {
                    eprintln!("[Relay] Connection error (port {}): {}", client_port, e);
                }
            });
        }
    }

    async fn handle_connection(
        mut client_stream: TcpStream,
        client_port: u16,
        cfg: ProxyConfig,
        active_conns: Arc<RwLock<Vec<ConnectionInfo>>>,
        total_sent: Arc<AtomicU64>,
        total_recv: Arc<AtomicU64>,
    ) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
        let mut buf = [0u8; 4096];
        let n = client_stream.peek(&mut buf).await?;
        if n == 0 {
            return Ok(());
        }

        let peeked = &buf[..n];
        let (target_host, target_port) = Self::parse_target(peeked);

        // Connect to upstream proxy
        let upstream_addr = format!("{}:{}", cfg.proxy_ip, cfg.proxy_port);
        let mut upstream_stream = TcpStream::connect(&upstream_addr).await?;

        // Authenticate with upstream HTTP proxy
        let auth_raw = format!("{}:{}", cfg.proxy_user, cfg.proxy_pass);
        let auth_b64 = base64::Engine::encode(&base64::engine::general_purpose::STANDARD, auth_raw);

        let connect_req = format!(
            "CONNECT {}:{} HTTP/1.1\r\nHost: {}:{}\r\nProxy-Authorization: Basic {}\r\nProxy-Connection: Keep-Alive\r\nConnection: Keep-Alive\r\n\r\n",
            target_host, target_port, target_host, target_port, auth_b64
        );

        upstream_stream.write_all(connect_req.as_bytes()).await?;

        // Read response header
        let mut resp_buf = [0u8; 1024];
        let resp_n = upstream_stream.read(&mut resp_buf).await?;
        let resp_str = String::from_utf8_lossy(&resp_buf[..resp_n]);

        if !resp_str.contains("200") {
            return Err(format!("Proxy CONNECT failed: {}", resp_str.lines().next().unwrap_or("")).into());
        }

        // If client sent an explicit CONNECT, reply 200 OK
        if peeked.starts_with(b"CONNECT ") {
            let mut read_dummy = [0u8; 4096];
            let _ = client_stream.read(&mut read_dummy).await?;
            let ok_resp = b"HTTP/1.1 200 Connection Established\r\n\r\n";
            client_stream.write_all(ok_resp).await?;
        }

        // Register connection info
        let conn_info = ConnectionInfo {
            client_port,
            target_host: target_host.clone(),
            target_port,
            bytes_sent: 0,
            bytes_recv: 0,
            start_time_secs: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap_or_default()
                .as_secs(),
        };

        {
            let mut conns = active_conns.write().await;
            conns.push(conn_info);
        }

        // Bidirectional pump
        let (mut cr, mut cw) = client_stream.into_split();
        let (mut ur, mut uw) = upstream_stream.into_split();

        let t_sent = total_sent.clone();
        let t_recv = total_recv.clone();

        let client_to_upstream = tokio::spawn(async move {
            let res = tokio::io::copy(&mut cr, &mut uw).await;
            if let Ok(bytes) = res {
                t_sent.fetch_add(bytes, Ordering::Relaxed);
            }
        });

        let upstream_to_client = tokio::spawn(async move {
            let res = tokio::io::copy(&mut ur, &mut cw).await;
            if let Ok(bytes) = res {
                t_recv.fetch_add(bytes, Ordering::Relaxed);
            }
        });

        let _ = tokio::join!(client_to_upstream, upstream_to_client);

        // Unregister connection info
        {
            let mut conns = active_conns.write().await;
            conns.retain(|c| c.client_port != client_port);
        }

        Ok(())
    }

    fn parse_target(buf: &[u8]) -> (String, u16) {
        // Try Extracting TLS SNI
        if let Some(sni) = Self::extract_sni(buf) {
            return (sni, 443);
        }

        // Try HTTP CONNECT header
        if let Ok(s) = std::str::from_utf8(buf) {
            if let Some(line) = s.lines().next() {
                let parts: Vec<&str> = line.split_whitespace().collect();
                if parts.len() >= 2 {
                    if parts[0] == "CONNECT" {
                        let hp: Vec<&str> = parts[1].split(':').collect();
                        let host = hp[0].to_string();
                        let port = hp.get(1).and_then(|p| p.parse().ok()).unwrap_or(443);
                        return (host, port);
                    } else if parts[1].starts_with("http://") {
                        if let Ok(url) = std::str::from_utf8(&buf[7..]) {
                            let host_part = url.split('/').next().unwrap_or("");
                            let hp: Vec<&str> = host_part.split(':').collect();
                            let host = hp[0].to_string();
                            let port = hp.get(1).and_then(|p| p.parse().ok()).unwrap_or(80);
                            return (host, port);
                        }
                    }
                }
            }
        }

        ("1.1.1.1".to_string(), 443)
    }

    fn extract_sni(buf: &[u8]) -> Option<String> {
        if buf.len() < 5 { return None; }
        if buf[0] != 0x16 || buf[1] != 0x03 { return None; }

        let record_len = ((buf[3] as usize) << 8) | (buf[4] as usize);
        if buf.len() < 5 + record_len { return None; }

        let mut pos = 5;
        if buf[pos] != 0x01 { return None; } // Client Hello
        pos += 4 + 2 + 32; // Skip header, version, random

        if pos >= buf.len() { return None; }
        let sess_len = buf[pos] as usize;
        pos += 1 + sess_len;

        if pos + 2 > buf.len() { return None; }
        let cipher_len = ((buf[pos] as usize) << 8) | (buf[pos + 1] as usize);
        pos += 2 + cipher_len;

        if pos + 1 > buf.len() { return None; }
        let comp_len = buf[pos] as usize;
        pos += 1 + comp_len;

        if pos + 2 > buf.len() { return None; }
        let ext_len = ((buf[pos] as usize) << 8) | (buf[pos + 1] as usize);
        pos += 2;

        let end_ext = (pos + ext_len).min(buf.len());

        while pos + 4 <= end_ext {
            let ext_type = ((buf[pos] as u16) << 8) | (buf[pos + 1] as u16);
            let ext_data_len = ((buf[pos + 2] as usize) << 8) | (buf[pos + 3] as usize);
            pos += 4;

            if ext_type == 0x0000 && pos + 5 <= end_ext { // SNI
                let name_type = buf[pos + 2];
                if name_type == 0 {
                    let name_len = ((buf[pos + 3] as usize) << 8) | (buf[pos + 4] as usize);
                    if pos + 5 + name_len <= end_ext {
                        return String::from_utf8(buf[pos + 5..pos + 5 + name_len].to_vec()).ok();
                    }
                }
            }
            pos += ext_data_len;
        }

        None
    }
}
