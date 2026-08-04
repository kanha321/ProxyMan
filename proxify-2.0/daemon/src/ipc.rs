use common::{IpcRequest, IpcResponse, DaemonState};
use std::sync::Arc;
use tokio::sync::RwLock;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::windows::named_pipe::{ServerOptions, NamedPipeServer};

const PIPE_NAME: &str = r"\\.\pipe\proxyman_ipc";

pub struct IpcServer {
    state: Arc<RwLock<DaemonState>>,
}

impl IpcServer {
    pub fn new(state: Arc<RwLock<DaemonState>>) -> Self {
        Self { state }
    }

    pub async fn run(&self) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
        println!("[IPC] Server listening on named pipe: {}", PIPE_NAME);

        let mut first = true;
        loop {
            let server = if first {
                first = false;
                ServerOptions::new().first_pipe_instance(true).create(PIPE_NAME)?
            } else {
                ServerOptions::new().create(PIPE_NAME)?
            };

            server.connect().await?;
            let state = self.state.clone();

            tokio::spawn(async move {
                if let Err(e) = Self::handle_client(server, state).await {
                    eprintln!("[IPC] Client error: {}", e);
                }
            });
        }
    }

    async fn handle_client(
        server: NamedPipeServer,
        state: Arc<RwLock<DaemonState>>,
    ) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
        let (read_half, mut write_half) = tokio::io::split(server);
        let mut reader = BufReader::new(read_half);
        let mut line = String::new();

        while reader.read_line(&mut line).await? > 0 {
            let req: Result<IpcRequest, _> = serde_json::from_str(line.trim());
            let resp = match req {
                Ok(IpcRequest::GetState) => {
                    let st = state.read().await.clone();
                    IpcResponse::State(st)
                }
                Ok(IpcRequest::StartEngine) => {
                    let mut st = state.write().await;
                    st.status = common::EngineStatus::Running;
                    IpcResponse::Success("Engine started".to_string())
                }
                Ok(IpcRequest::StopEngine) => {
                    let mut st = state.write().await;
                    st.status = common::EngineStatus::Stopped;
                    IpcResponse::Success("Engine stopped".to_string())
                }
                Ok(IpcRequest::SetConfig(cfg)) => {
                    let mut st = state.write().await;
                    st.config = cfg;
                    IpcResponse::Success("Config updated".to_string())
                }
                Err(e) => IpcResponse::Error(format!("Invalid JSON request: {}", e)),
            };

            let mut resp_bytes = serde_json::to_vec(&resp)?;
            resp_bytes.push(b'\n');
            write_half.write_all(&resp_bytes).await?;
            line.clear();
        }

        Ok(())
    }
}
