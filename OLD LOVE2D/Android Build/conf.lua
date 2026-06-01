function love.conf(t)
    t.window.title   = "Planet Tracker"
    t.window.width   = 1100
    t.window.height  = 720
    t.window.resizable = false
    t.window.vsync   = 1
    t.window.minwidth  = 1100
    t.window.minheight = 720

    t.modules.audio   = false   
    t.modules.joystick = false
    t.modules.physics  = false
    t.modules.video    = false

    t.version = "11.4"
    t.console = true
end