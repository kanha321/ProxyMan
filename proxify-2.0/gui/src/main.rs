mod app;
mod ipc_client;
mod spring_physics;
mod spring_rect;
mod themes;

use app::ProxyBridgeApp;
use eframe::NativeOptions;

fn main() -> Result<(), eframe::Error> {
    let options = NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_inner_size([720.0, 520.0])
            .with_min_inner_size([600.0, 400.0])
            .with_title("ProxyMan v2.0 — Desktop Dashboard"),
        ..Default::default()
    };

    eframe::run_native(
        "ProxyMan",
        options,
        Box::new(|cc| Ok(Box::new(ProxyBridgeApp::new(cc)))),
    )
}
