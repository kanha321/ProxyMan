use crate::spring_rect::SpringRect;
use crate::themes::{Theme, ThemeColors};
use common::{DaemonState, EngineStatus, LinkType, ProxyConfig};
use eframe::App;
use egui::Vec2;
use std::sync::Arc;
use tokio::sync::RwLock;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Tab {
    Overview,
    Bandwidth,
    Connections,
    Settings,
    Themes,
}

pub struct ProxyBridgeApp {
    current_tab: Tab,
    theme: Theme,
    selection_spring: Option<SpringRect>,
    state: Arc<RwLock<Option<DaemonState>>>,
    config_draft: ProxyConfig,
    rt: tokio::runtime::Runtime,
}

impl ProxyBridgeApp {
    pub fn new(_cc: &eframe::CreationContext<'_>) -> Self {
        let rt = tokio::runtime::Builder::new_multi_thread()
            .enable_all()
            .build()
            .unwrap();

        let state = Arc::new(RwLock::new(None));
        let state_clone = state.clone();

        // Spawn background polling task to fetch daemon state via IPC
        rt.spawn(async move {
            loop {
                let st = crate::ipc_client::fetch_daemon_state().await;
                {
                    let mut w = state_clone.write().await;
                    *w = st;
                }
                tokio::time::sleep(tokio::time::Duration::from_millis(500)).await;
            }
        });

        Self {
            current_tab: Tab::Overview,
            theme: Theme::CatppuccinMocha,
            selection_spring: None,
            state,
            config_draft: ProxyConfig::default(),
            rt,
        }
    }
}

impl App for ProxyBridgeApp {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let colors = self.theme.colors();
        let mut visual_style = (*ctx.style()).clone();
        visual_style.visuals.override_text_color = Some(colors.text);
        visual_style.visuals.panel_fill = colors.bg;
        visual_style.visuals.window_fill = colors.card_bg;
        ctx.set_style(visual_style);

        // Sidebar Panel
        egui::SidePanel::left("sidebar_panel")
            .exact_width(180.0)
            .show(ctx, |ui| {
                ui.add_space(15.0);
                ui.vertical_centered(|ui| {
                    ui.heading(egui::RichText::new("⚡ ProxyMan").size(20.0).strong().color(colors.accent));
                    ui.label(egui::RichText::new("v2.0 Rust Edition").size(11.0).color(colors.subtext));
                });
                ui.add_space(20.0);

                let tabs = [
                    (Tab::Overview, "📊 Overview"),
                    (Tab::Bandwidth, "📈 Bandwidth"),
                    (Tab::Connections, "🔌 Connections"),
                    (Tab::Settings, "⚙ Settings"),
                    (Tab::Themes, "🎨 Themes"),
                ];

                for (tab, label) in tabs {
                    let is_selected = self.current_tab == tab;
                    let text = if is_selected {
                        egui::RichText::new(label).strong().color(colors.accent)
                    } else {
                        egui::RichText::new(label).color(colors.text)
                    };

                    if ui.selectable_label(is_selected, text).clicked() {
                        self.current_tab = tab;
                    }
                }
            });

        // Main Central Panel
        egui::CentralPanel::default().show(ctx, |ui| {
            let st = self.state.blocking_read().clone();

            match self.current_tab {
                Tab::Overview => self.show_overview(ui, st.as_ref(), &colors),
                Tab::Bandwidth => self.show_bandwidth(ui, st.as_ref(), &colors),
                Tab::Connections => self.show_connections(ui, st.as_ref(), &colors),
                Tab::Settings => self.show_settings(ui, st.as_ref(), &colors),
                Tab::Themes => self.show_themes(ui, &colors),
            }
        });

        // Request continuous repaint for smooth 60fps spring animations
        ctx.request_repaint();
    }
}

impl ProxyBridgeApp {
    fn show_overview(&mut self, ui: &mut egui::Ui, state: Option<&DaemonState>, colors: &ThemeColors) {
        ui.heading(egui::RichText::new("Status Overview").size(22.0).strong());
        ui.add_space(15.0);

        egui::Grid::new("overview_cards").spacing(Vec2::new(20.0, 20.0)).show(ui, |ui| {
            // Engine Status Card
            ui.group(|ui| {
                ui.set_min_size(Vec2::new(220.0, 100.0));
                ui.vertical(|ui| {
                    ui.label(egui::RichText::new("Engine Status").size(13.0).color(colors.subtext));
                    ui.add_space(5.0);
                    if let Some(st) = state {
                        match &st.status {
                            EngineStatus::Running => {
                                ui.label(egui::RichText::new("● RUNNING").size(18.0).strong().color(colors.success));
                            }
                            EngineStatus::Stopped => {
                                ui.label(egui::RichText::new("○ STOPPED").size(18.0).strong().color(colors.subtext));
                            }
                            EngineStatus::Starting => {
                                ui.label(egui::RichText::new("⏳ STARTING").size(18.0).strong().color(colors.warning));
                            }
                            EngineStatus::Stopping => {
                                ui.label(egui::RichText::new("⏳ STOPPING").size(18.0).strong().color(colors.warning));
                            }
                            EngineStatus::Error(e) => {
                                ui.label(egui::RichText::new(format!("❌ ERROR: {}", e)).size(14.0).strong().color(colors.error));
                            }
                        }
                    } else {
                        ui.label(egui::RichText::new("○ DAEMON OFFLINE").size(16.0).strong().color(colors.error));
                    }
                });
            });

            // Active Network Card
            ui.group(|ui| {
                ui.set_min_size(Vec2::new(220.0, 100.0));
                ui.vertical(|ui| {
                    ui.label(egui::RichText::new("Active Network Link").size(13.0).color(colors.subtext));
                    ui.add_space(5.0);
                    if let Some(st) = state {
                        let icon = match st.active_link {
                            LinkType::Ethernet => "🔌 Ethernet",
                            LinkType::WiFi => "📡 Wi-Fi",
                            LinkType::Other => "🌐 Other",
                            LinkType::Unknown => "❓ Unknown",
                        };
                        ui.label(egui::RichText::new(icon).size(18.0).strong().color(colors.accent));
                    } else {
                        ui.label(egui::RichText::new("N/A").size(18.0).strong().color(colors.subtext));
                    }
                });
            });

            ui.end_row();

            // Active Connections Card
            ui.group(|ui| {
                ui.set_min_size(Vec2::new(220.0, 100.0));
                ui.vertical(|ui| {
                    ui.label(egui::RichText::new("Active Connections").size(13.0).color(colors.subtext));
                    ui.add_space(5.0);
                    let count = state.map(|s| s.active_connections.len()).unwrap_or(0);
                    ui.label(egui::RichText::new(format!("{} Flows", count)).size(22.0).strong().color(colors.accent));
                });
            });

            // Upstream Target Card
            ui.group(|ui| {
                ui.set_min_size(Vec2::new(220.0, 100.0));
                ui.vertical(|ui| {
                    ui.label(egui::RichText::new("Upstream Target").size(13.0).color(colors.subtext));
                    ui.add_space(5.0);
                    if let Some(st) = state {
                        ui.label(egui::RichText::new(format!("{}:{}", st.config.proxy_ip, st.config.proxy_port)).size(16.0).strong().color(colors.text));
                    } else {
                        ui.label(egui::RichText::new("172.31.100.25:3128").size(16.0).strong().color(colors.text));
                    }
                });
            });

            ui.end_row();
        });

        ui.add_space(30.0);
        ui.horizontal(|ui| {
            if ui.button(egui::RichText::new(" ▶ Start Engine ").size(14.0)).clicked() {
                self.rt.spawn(async {
                    let _ = crate::ipc_client::send_ipc_request(common::IpcRequest::StartEngine).await;
                });
            }
            if ui.button(egui::RichText::new(" ⏹ Stop Engine ").size(14.0)).clicked() {
                self.rt.spawn(async {
                    let _ = crate::ipc_client::send_ipc_request(common::IpcRequest::StopEngine).await;
                });
            }
        });
    }

    fn show_bandwidth(&mut self, ui: &mut egui::Ui, state: Option<&DaemonState>, colors: &ThemeColors) {
        ui.heading(egui::RichText::new("Bandwidth & Traffic Meter").size(22.0).strong());
        ui.add_space(15.0);

        let sent = state.map(|s| s.total_bytes_sent).unwrap_or(0);
        let recv = state.map(|s| s.total_bytes_recv).unwrap_or(0);

        ui.horizontal(|ui| {
            ui.label(egui::RichText::new(format!("Total Upload: {:.2} MB", sent as f64 / 1_048_576.0)).size(16.0).color(colors.accent));
            ui.add_space(30.0);
            ui.label(egui::RichText::new(format!("Total Download: {:.2} MB", recv as f64 / 1_048_576.0)).size(16.0).color(colors.success));
        });
    }

    fn show_connections(&mut self, ui: &mut egui::Ui, state: Option<&DaemonState>, colors: &ThemeColors) {
        ui.heading(egui::RichText::new("Active Connection Inspector").size(22.0).strong());
        ui.add_space(15.0);

        egui::ScrollArea::vertical().show(ui, |ui| {
            egui::Grid::new("conns_table").striped(true).spacing(Vec2::new(15.0, 10.0)).show(ui, |ui| {
                ui.label(egui::RichText::new("Client Port").strong());
                ui.label(egui::RichText::new("Target Host").strong());
                ui.label(egui::RichText::new("Target Port").strong());
                ui.label(egui::RichText::new("Status").strong());
                ui.end_row();

                if let Some(st) = state {
                    for conn in &st.active_connections {
                        ui.label(conn.client_port.to_string());
                        ui.label(&conn.target_host);
                        ui.label(conn.target_port.to_string());
                        ui.label(egui::RichText::new("Active").color(colors.success));
                        ui.end_row();
                    }
                }
            });
        });
    }

    fn show_settings(&mut self, ui: &mut egui::Ui, _state: Option<&DaemonState>, _colors: &ThemeColors) {
        ui.heading(egui::RichText::new("Proxy Configuration Settings").size(22.0).strong());
        ui.add_space(15.0);

        egui::Grid::new("settings_grid").spacing(Vec2::new(15.0, 15.0)).show(ui, |ui| {
            ui.label("Proxy Server IP:");
            ui.text_edit_singleline(&mut self.config_draft.proxy_ip);
            ui.end_row();

            ui.label("Proxy Port:");
            ui.add(egui::DragValue::new(&mut self.config_draft.proxy_port));
            ui.end_row();

            ui.label("Username:");
            ui.text_edit_singleline(&mut self.config_draft.proxy_user);
            ui.end_row();

            ui.label("Password:");
            ui.text_edit_singleline(&mut self.config_draft.proxy_pass);
            ui.end_row();

            ui.label("Local Relay Port:");
            ui.add(egui::DragValue::new(&mut self.config_draft.relay_port));
            ui.end_row();
        });

        ui.add_space(20.0);
        if ui.button(egui::RichText::new(" 💾 Save Configuration ").size(14.0)).clicked() {
            let cfg = self.config_draft.clone();
            self.rt.spawn(async move {
                let _ = crate::ipc_client::send_ipc_request(common::IpcRequest::SetConfig(cfg)).await;
            });
        }
    }

    fn show_themes(&mut self, ui: &mut egui::Ui, _colors: &ThemeColors) {
        ui.heading(egui::RichText::new("Theme Selection").size(22.0).strong());
        ui.add_space(15.0);

        let themes = [
            (Theme::CatppuccinMocha, "Catppuccin Mocha"),
            (Theme::TokyoNight, "Tokyo Night"),
            (Theme::GruvboxDark, "Gruvbox Dark"),
            (Theme::Dracula, "Dracula"),
            (Theme::MonokaiPro, "Monokai Pro"),
        ];

        for (theme, name) in themes {
            if ui.radio_value(&mut self.theme, theme, name).clicked() {
                self.theme = theme;
            }
        }
    }
}
