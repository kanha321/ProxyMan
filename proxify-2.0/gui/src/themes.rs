use egui::Color32;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Theme {
    TokyoNight,
    CatppuccinMocha,
    GruvboxDark,
    Dracula,
    MonokaiPro,
}

pub struct ThemeColors {
    pub bg: Color32,
    pub card_bg: Color32,
    pub sidebar_bg: Color32,
    pub accent: Color32,
    pub text: Color32,
    pub subtext: Color32,
    pub success: Color32,
    pub warning: Color32,
    pub error: Color32,
}

impl Theme {
    pub fn colors(&self) -> ThemeColors {
        match self {
            Theme::TokyoNight => ThemeColors {
                bg: Color32::from_rgb(26, 27, 38),
                card_bg: Color32::from_rgb(36, 40, 59),
                sidebar_bg: Color32::from_rgb(16, 17, 26),
                accent: Color32::from_rgb(122, 162, 247),
                text: Color32::from_rgb(190, 205, 240),
                subtext: Color32::from_rgb(86, 95, 137),
                success: Color32::from_rgb(158, 206, 106),
                warning: Color32::from_rgb(224, 175, 104),
                error: Color32::from_rgb(247, 118, 142),
            },
            Theme::CatppuccinMocha => ThemeColors {
                bg: Color32::from_rgb(30, 30, 46),
                card_bg: Color32::from_rgb(49, 50, 68),
                sidebar_bg: Color32::from_rgb(17, 17, 27),
                accent: Color32::from_rgb(137, 180, 250),
                text: Color32::from_rgb(205, 214, 244),
                subtext: Color32::from_rgb(166, 173, 200),
                success: Color32::from_rgb(166, 227, 161),
                warning: Color32::from_rgb(249, 226, 175),
                error: Color32::from_rgb(243, 139, 168),
            },
            Theme::GruvboxDark => ThemeColors {
                bg: Color32::from_rgb(40, 40, 40),
                card_bg: Color32::from_rgb(60, 56, 54),
                sidebar_bg: Color32::from_rgb(29, 32, 33),
                accent: Color32::from_rgb(254, 128, 25),
                text: Color32::from_rgb(235, 219, 178),
                subtext: Color32::from_rgb(168, 153, 132),
                success: Color32::from_rgb(184, 187, 38),
                warning: Color32::from_rgb(250, 189, 47),
                error: Color32::from_rgb(251, 73, 52),
            },
            Theme::Dracula => ThemeColors {
                bg: Color32::from_rgb(40, 42, 54),
                card_bg: Color32::from_rgb(68, 71, 90),
                sidebar_bg: Color32::from_rgb(33, 34, 44),
                accent: Color32::from_rgb(189, 147, 249),
                text: Color32::from_rgb(248, 248, 242),
                subtext: Color32::from_rgb(98, 114, 164),
                success: Color32::from_rgb(80, 250, 123),
                warning: Color32::from_rgb(241, 250, 140),
                error: Color32::from_rgb(255, 85, 85),
            },
            Theme::MonokaiPro => ThemeColors {
                bg: Color32::from_rgb(45, 42, 46),
                card_bg: Color32::from_rgb(64, 60, 65),
                sidebar_bg: Color32::from_rgb(34, 31, 34),
                accent: Color32::from_rgb(255, 216, 102),
                text: Color32::from_rgb(252, 252, 250),
                subtext: Color32::from_rgb(114, 112, 114),
                success: Color32::from_rgb(169, 220, 118),
                warning: Color32::from_rgb(255, 97, 136),
                error: Color32::from_rgb(255, 97, 136),
            },
        }
    }
}
