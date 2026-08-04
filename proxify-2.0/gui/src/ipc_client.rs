use common::{IpcRequest, IpcResponse, DaemonState};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::windows::named_pipe::ClientOptions;

const PIPE_NAME: &str = r"\\.\pipe\proxyman_ipc";

pub async fn send_ipc_request(req: IpcRequest) -> Result<IpcResponse, Box<dyn std::error::Error + Send + Sync>> {
    let client = ClientOptions::new().open(PIPE_NAME)?;
    let (read_half, mut write_half) = tokio::io::split(client);

    let mut req_bytes = serde_json::to_vec(&req)?;
    req_bytes.push(b'\n');
    write_half.write_all(&req_bytes).await?;

    let mut reader = BufReader::new(read_half);
    let mut line = String::new();
    reader.read_line(&mut line).await?;

    let resp: IpcResponse = serde_json::from_str(line.trim())?;
    Ok(resp)
}

pub async fn fetch_daemon_state() -> Option<DaemonState> {
    match send_ipc_request(IpcRequest::GetState).await {
        Ok(IpcResponse::State(st)) => Some(st),
        _ => None,
    }
}
