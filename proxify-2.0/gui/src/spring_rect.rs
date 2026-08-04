use crate::spring_physics::Spring;
use egui::Rect;

#[derive(Debug, Clone)]
pub struct SpringRect {
    pub min_x: Spring,
    pub min_y: Spring,
    pub max_x: Spring,
    pub max_y: Spring,
}

impl SpringRect {
    pub fn new(initial_rect: Rect, stiffness: f32, damping: f32) -> Self {
        Self {
            min_x: Spring::new(initial_rect.min.x, stiffness, damping),
            min_y: Spring::new(initial_rect.min.y, stiffness, damping),
            max_x: Spring::new(initial_rect.max.x, stiffness, damping),
            max_y: Spring::new(initial_rect.max.y, stiffness, damping),
        }
    }

    pub fn set_target(&mut self, target: Rect) {
        self.min_x.set_target(target.min.x);
        self.min_y.set_target(target.min.y);
        self.max_x.set_target(target.max.x);
        self.max_y.set_target(target.max.y);
    }

    pub fn update(&mut self, dt: f32) {
        self.min_x.update(dt);
        self.min_y.update(dt);
        self.max_x.update(dt);
        self.max_y.update(dt);
    }

    pub fn value(&self) -> Rect {
        Rect::from_min_max(
            egui::pos2(self.min_x.position, self.min_y.position),
            egui::pos2(self.max_x.position, self.max_y.position),
        )
    }
}
