mod ipc;
mod net_watcher;
mod proxy_settings;
mod relay;
mod packet_engine;

use common::{DaemonState, EngineStatus, ProxyConfig, LinkType};
use ipc::IpcServer;
use net_watcher::get_active_link_type;
use relay::RelayServer;
use packet_engine::PacketEngine;
use proxy_settings::{set_system_proxy, clear_system_proxy};

use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use tokio::sync::RwLock;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    println!("==========================================================");
    println!("  ProxyBridge Daemon v2.0 (Rust Service Engine)");
    println!("==========================================================");

    let config = ProxyConfig::default();
    let active_link = get_active_link_type();
    println!("[Daemon] Initial network detected: {}", active_link);

    let initial_status = if active_link == LinkType::Ethernet {
        println!("[Daemon] Ethernet detected - enabling system proxy...");
        set_system_proxy(&format!("127.0.0.1:{}", config.relay_port));
        EngineStatus::Running
    } else {
        println!("[Daemon] Not on Ethernet - staying idle.");
        clear_system_proxy();
        EngineStatus::Stopped
    };

    let state = Arc::new(RwLock::new(DaemonState {
        status: initial_status,
        active_link: active_link.clone(),
        config: config.clone(),
        active_connections: Vec::new(),
        total_bytes_sent: 0,
        total_bytes_recv: 0,
        quic_blocked_count: 0,
    }));

    let active_conns = Arc::new(RwLock::new(Vec::new()));
    let total_sent = Arc::new(AtomicU64::new(0));
    let total_recv = Arc::new(AtomicU64::new(0));
    let stopping = Arc::new(AtomicBool::new(false));

    // Launch IPC server
    let ipc_server = IpcServer::new(state.clone());
    tokio::spawn(async move {
        if let Err(e) = ipc_server.run().await {
            eprintln!("[IPC] Server error: {}", e);
        }
    });

    // Launch Relay server & PacketEngine if Ethernet is active
    if active_link == LinkType::Ethernet {
        let relay = RelayServer::new(config.clone(), active_conns.clone(), total_sent.clone(), total_recv.clone());
        tokio::spawn(async move {
            if let Err(e) = relay.run().await {
                eprintln!("[Relay] Server error: {}", e);
            }
        });

        let pe_quic = PacketEngine::new(config.clone(), stopping.clone());
        std::thread::spawn(move || {
            if let Err(e) = pe_quic.run_quic_blocker() {
                eprintln!("[QuicBlocker] Error: {}", e);
            }
        });

        let pe_nat = PacketEngine::new(config.clone(), stopping.clone());
        std::thread::spawn(move || {
            if let Err(e) = pe_nat.run_tcp_redirect() {
                eprintln!("[PacketEngine] Error: {}", e);
            }
        });
    }

    println!("\n[Daemon] ProxyBridge Daemon running. Press Ctrl+C to exit.\n");

    tokio::signal::ctrl_c().await?;
    println!("[Daemon] Shutdown signal received...");

    stopping.store(true, Ordering::Relaxed);
    clear_system_proxy();
    println!("[Daemon] Clean shutdown complete. Goodbye.");

    Ok(())
}
