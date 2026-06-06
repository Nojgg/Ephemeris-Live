class Star {
    constructor(canvasWidth, canvasHeight) {
        this.reset(canvasWidth, canvasHeight);
    }

    reset(width, height) {
        this.x = Math.random() * width;
        this.y = Math.random() * height;
        this.size = Math.random() * 1.5 + 0.5;
        this.opacity = Math.random();
        this.twinkleSpeed = Math.random() * 0.02 + 0.005;
        this.direction = Math.random() > 0.5 ? 1 : -1;
        this.depth = Math.random() * 0.5 + 0.1;
    }

    update() {
        // Add the twinkle effect back
        this.opacity += this.twinkleSpeed * this.direction;
        if (this.opacity > 1 || this.opacity < 0.1) this.direction *= -1;
    }

    draw(ctx, mouseX, mouseY, width, height) {
        const offsetX = (mouseX - width / 2) * this.depth * 0.1;
        const offsetY = (mouseY - height / 2) * this.depth * 0.1;

        ctx.fillStyle = `rgba(255, 255, 255, ${this.opacity})`;
        ctx.beginPath();
        ctx.arc(this.x + offsetX, this.y + offsetY, this.size, 0, Math.PI * 2);
        ctx.fill();
    }
}

function Starfield() {
    this.stars = [];
    this.canvas = null;
    this.ctx = null;
    this.mouseX = 0;
    this.mouseY = 0;

    this.initialise = function (container) {
        this.canvas = document.createElement('canvas');
        this.ctx = this.canvas.getContext('2d');
        container.appendChild(this.canvas);

        this.resize();
        window.addEventListener('resize', () => this.resize());

        window.addEventListener('mousemove', (e) => {
            this.mouseX = e.clientX;
            this.mouseY = e.clientY;
        });

        for (let i = 0; i < 400; i++) {
            this.stars.push(new Star(window.innerWidth, window.innerHeight));
        }
    };

    this.resize = function () {
        this.canvas.width = window.innerWidth;
        this.canvas.height = window.innerHeight;
    };

    this.setupScrollFade = function () {
        // We target the canvas element directly for the fade
        window.addEventListener('scroll', () => {
            let scrollY = window.scrollY;
            let opacity = 1 - (scrollY / 500);
            this.canvas.style.opacity = Math.max(0, Math.min(1, opacity));
        });
    };

    this.start = function () {
        const animate = () => {
            this.ctx.fillStyle = '#050505';
            this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

            this.stars.forEach(star => {
                star.update(); // Keep them twinkling
                star.draw(this.ctx, this.mouseX, this.mouseY, this.canvas.width, this.canvas.height);
            });
            requestAnimationFrame(animate);
        };
        animate();
    };
}