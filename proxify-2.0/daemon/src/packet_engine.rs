use common::ProxyConfig;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use windivert::prelude::*;

pub struct PacketEngine {
    config: ProxyConfig,
    stopping: Arc<AtomicBool>,
}

impl PacketEngine {
    pub fn new(config: ProxyConfig, stopping: Arc<AtomicBool>) -> Self {
        Self { config, stopping }
    }

    pub fn run_quic_blocker(&self) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
        let filter = "outbound and udp and udp.DstPort == 443";
        let divert = WinDivert::network(filter, 0, WinDivertFlags::new())?;
        println!("[QuicBlocker] Started - dropping outbound UDP:443 (forces TCP fallback)");

        let mut buf = vec![0u8; 65535];
        while !self.stopping.load(Ordering::Relaxed) {
            match divert.recv(Some(&mut buf)) {
                Ok(_packet) => {
                    // Intentionally drop: do NOT send back
                }
                Err(_) => {
                    if self.stopping.load(Ordering::Relaxed) { break; }
                }
            }
        }

        println!("[QuicBlocker] Stopped.");
        Ok(())
    }

    pub fn run_tcp_redirect(&self) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
        let filter = format!(
            "tcp and ((outbound and !loopback) or (outbound and loopback and tcp.SrcPort == {}))",
            self.config.relay_port
        );

        let divert = WinDivert::network(&filter, 0, WinDivertFlags::new())?;
        println!("[PacketEngine] Started - intercepting TCP flows to 127.0.0.1:{}", self.config.relay_port);

        let mut buf = vec![0u8; 65535];
        while !self.stopping.load(Ordering::Relaxed) {
            match divert.recv(Some(&mut buf)) {
                Ok(packet) => {
                    let is_loopback = packet.address.loopback();

                    if let Ok(parsed) = etherparse::SlicedPacket::from_ip(&packet.data) {
                        if let (Some(etherparse::InternetSlice::Ipv4(ip_hdr, _)), Some(etherparse::TransportSlice::Tcp(tcp_hdr))) =
                            (&parsed.ip, &parsed.transport)
                        {
                            let src_port = tcp_hdr.source_port();
                            let is_return_leg = is_loopback && src_port == self.config.relay_port;

                            if !is_return_leg {
                                let _ = ip_hdr;
                            }
                        }
                    }

                    // Recalculate checksums and reinject
                    let _ = divert.send(&packet);
                }
                Err(_) => {
                    if self.stopping.load(Ordering::Relaxed) { break; }
                }
            }
        }

        println!("[PacketEngine] Stopped.");
        Ok(())
    }
}
