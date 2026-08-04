#[derive(Debug, Clone, Copy)]
pub struct Spring {
    pub position: f32,
    pub velocity: f32,
    pub target: f32,
    pub stiffness: f32,
    pub damping: f32,
}

impl Spring {
    pub fn new(initial_value: f32, stiffness: f32, damping: f32) -> Self {
        Self {
            position: initial_value,
            velocity: 0.0,
            target: initial_value,
            stiffness,
            damping,
        }
    }

    pub fn set_target(&mut self, target: f32) {
        self.target = target;
    }

    pub fn update(&mut self, dt: f32) {
        if dt <= 0.0 {
            return;
        }

        // Analytical spring solver (F = -k * x - c * v)
        let x = self.position - self.target;
        let k = self.stiffness;
        let c = self.damping;

        let damping_ratio = c / (2.0 * (k).sqrt());

        if damping_ratio < 1.0 {
            // Underdamped (bouncy)
            let omega_d = (k - c * c / 4.0).sqrt();
            let alpha = c / 2.0;

            let a = x;
            let b = (self.velocity + alpha * x) / omega_d;

            let exp_term = (-alpha * dt).exp();
            let cos_term = (omega_d * dt).cos();
            let sin_term = (omega_d * dt).sin();

            self.position = self.target + exp_term * (a * cos_term + b * sin_term);
            self.velocity = exp_term * ((b * omega_d - a * alpha) * cos_term - (a * omega_d + b * alpha) * sin_term);
        } else {
            // Critically damped or overdamped
            let force = -k * x - c * self.velocity;
            self.velocity += force * dt;
            self.position += self.velocity * dt;
        }
    }
}
