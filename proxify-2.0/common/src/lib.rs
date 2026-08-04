use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum LinkType {
    Ethernet,
    WiFi,
    Other,
    Unknown,
}

impl std::fmt::Display for LinkType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            LinkType::Ethernet => write!(f, "Ethernet"),
            LinkType::WiFi => write!(f, "Wi-Fi"),
            LinkType::Other => write!(f, "Other"),
            LinkType::Unknown => write!(f, "Unknown"),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum EngineStatus {
    Stopped,
    Starting,
    Running,
    Stopping,
    Error(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProxyConfig {
    pub proxy_ip: String,
    pub proxy_port: u16,
    pub proxy_user: String,
    pub proxy_pass: String,
    pub relay_port: u16,
    pub auto_mode: bool,
}

impl Default for ProxyConfig {
    fn default() -> Self {
        Self {
            proxy_ip: "172.31.100.25".to_string(),
            proxy_port: 3128,
            proxy_user: "edcguest".to_string(),
            proxy_pass: "edcguest".to_string(),
            relay_port: 55555,
            auto_mode: true,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConnectionInfo {
    pub client_port: u16,
    pub target_host: String,
    pub target_port: u16,
    pub bytes_sent: u64,
    pub bytes_recv: u64,
    pub start_time_secs: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DaemonState {
    pub status: EngineStatus,
    pub active_link: LinkType,
    pub config: ProxyConfig,
    pub active_connections: Vec<ConnectionInfo>,
    pub total_bytes_sent: u64,
    pub total_bytes_recv: u64,
    pub quic_blocked_count: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", content = "payload")]
pub enum IpcRequest {
    GetState,
    StartEngine,
    StopEngine,
    SetConfig(ProxyConfig),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", content = "payload")]
pub enum IpcResponse {
    State(DaemonState),
    Success(String),
    Error(String),
}
