"""Lokal Tk-visning. Ingen ekstra Python-pakker er nødvendige."""
import math
import tkinter as tk
from tkinter import messagebox, ttk
from .config import load_config
from .simulation import Simulation


def launch(config_path=None):
    root = tk.Tk()
    root.title('LinjeFølger testbed – trinn 1: emulator')
    root.geometry('1150x780')
    config, c = load_config(config_path)
    sim = Simulation(config, c)
    running, trail = False, []
    toolbar = ttk.Frame(root, padding=8)
    toolbar.pack(fill='x')
    status = tk.StringVar()
    telemetry = tk.StringVar()
    canvas = tk.Canvas(root, background='#eef2f4', highlightthickness=0)
    canvas.pack(fill='both', expand=True)
    ttk.Label(root, textvariable=telemetry, font=('Consolas', 10), padding=8, justify='left').pack(fill='x')
    ttk.Label(root, text='Eksempelmål – ikke kalibrert mot fysisk robot. Endre JSON og velg «Last konfig på nytt».', padding=6).pack(fill='x')

    def toggle():
        nonlocal running
        running = not running

    def stop():
        nonlocal running
        sim.stop()
        running = False
        draw()

    def reset():
        nonlocal trail, running
        sim.reset()
        trail, running = [], False
        draw()

    def reload_config():
        nonlocal sim, trail, running
        try:
            new_config, new_c = load_config(config_path)
            sim = Simulation(new_config, new_c)
            trail, running = [], False
            draw()
        except (ValueError, OSError, KeyError) as exc:
            messagebox.showerror('Konfigurasjon kunne ikke lastes', str(exc))

    for label, command in [('Start / pause', toggle), ('Stopp', stop), ('Nullstill', reset), ('Last konfig på nytt', reload_config)]:
        ttk.Button(toolbar, text=label, command=command).pack(side='left', padx=3)
    ttk.Label(toolbar, textvariable=status).pack(side='right')
    root.bind('<space>', lambda event: toggle())

    def draw():
        canvas.delete('all')
        width, height = max(canvas.winfo_width(), 100), max(canvas.winfo_height(), 100)
        t, r = sim.track, sim.config['robot']
        extent_x = t.half_straight + t.radius + 0.22
        extent_y = t.radius + 0.22
        scale = min(width / (extent_x * 2), height / (extent_y * 2))

        def point(x, y):
            return width / 2 + x * scale, height / 2 - y * scale

        def line(points, **kwargs):
            coordinates = [value for x, y in points for value in point(x, y)]
            if len(coordinates) >= 4:
                canvas.create_line(*coordinates, **kwargs)

        line(t.points(), fill='#202833', width=max(1, t.width * scale), capstyle='round')
        line(trail, fill='#8498ab', width=1)
        pose = sim.robot.pose

        def local(x, y):
            return sim.sensors.transform(x, y, pose)

        # Kroppen er sentrert om hjulakselen i den foreløpige modellen.
        body = [local(x, y) for x, y in ((r['body_length_m']/2, r['body_width_m']/2),
                                        (r['body_length_m']/2, -r['body_width_m']/2),
                                        (-r['body_length_m']/2, -r['body_width_m']/2),
                                        (-r['body_length_m']/2, r['body_width_m']/2))]
        canvas.create_polygon(*[v for x, y in body for v in point(x, y)], fill='#429bb5', outline='#1a4050', width=2)
        for lateral in (r['wheelbase_m']/2, -r['wheelbase_m']/2):
            line([local(-r['wheel_diameter_m']/2, lateral), local(r['wheel_diameter_m']/2, lateral)], fill='#233344', width=7)
        line([local(0, 0), local(r['body_length_m']/2 + 0.015, 0)], fill='white', width=3, arrow=tk.LAST)
        sensor_positions = sim.sensors.positions(pose)
        line(sensor_positions, fill='#ea9f39', width=2)
        for i, ((x, y), value) in enumerate(zip(sensor_positions, sim.values)):
            sx, sy = point(x, y)
            color = '#%02x%02x%02x' % (int(255 * value / 1000), int(210 * (1-value/1000)), 55)
            canvas.create_oval(sx-4, sy-4, sx+4, sy+4, fill=color, outline='#293340')
            canvas.create_text(sx, sy-12, text=str(i), font=('Consolas', 8))
        o = sim.observation()
        if abs(o['x_m']) > extent_x or abs(o['y_m']) > extent_y:
            canvas.create_text(width/2, 25, text='Roboten er utenfor visningen – nullstill for å starte på banen.', fill='#a63827')
        status.set(('Kjører' if running else 'Pause') + f"  |  {o['time_ms']/1000:.2f} s  |  modus {o['mode']}")
        telemetry.set(
            f"Sensorer: {sim.values}    kvalitet {o['quality']}    feil {o['error']}\n"
            f"PWM-mål V/H: {o['left_command']:7.2f} / {o['right_command']:7.2f}    "
            f"RPM: {o['left_rpm']:7.2f} / {o['right_rpm']:7.2f}\n"
            f"Spenning: {o['voltage_v']:.2f} V   Strøm: {o['current_a']:.3f} A   "
            f"Fart: {o['velocity_m_s']:.3f} m/s   Avstand til linje: {o['line_distance_m']*1000:.1f} mm")

    def tick():
        nonlocal running
        if running:
            try:
                sim.advance(sim.config['simulation']['frame_ms'])
                trail.append((sim.robot.x, sim.robot.y))
                del trail[:-5000]
                if sim.control.stopped:
                    running = False
            except ValueError as exc:
                running = False
                messagebox.showerror('Simulering stoppet', str(exc))
        draw()
        root.after(sim.config['simulation']['frame_ms'], tick)

    root.after(50, tick)
    root.mainloop()
