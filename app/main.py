"""
==============================================================
  Quiz Buzzer Controller — V9 (Full Featured + Beautiful UI)
  Features: Game Timer, Time Limit, Activity Log, Pass Button
==============================================================
"""

import customtkinter as ctk
import asyncio, threading, json, time
import websockets
import winsound

APP_TITLE    = "Quiz Buzzer Controller"
DEFAULT_IP   = "192.168.4.1"
WS_PORT      = 81
PLAYER_NAMES = ["Buzzer 1","Buzzer 2","Buzzer 3","Buzzer 4","Buzzer 5","Buzzer 6"]

# ── Theme ───────────────────────────────────────────────────────
BG_MAIN        = "#ffffff"
BG_HEADER      = "#3C0561"
BG_CARD        = "#ffffff"
BG_LOG         = "#f9fafb"
C_ACCENT       = "#3C0561"
C_ACCENT_LIGHT = "#f3e8ff"
C_ACCENT_HOVER = "#550a8a"
C_ACCENT_TEXT  = "#ffffff"
C_TEXT         = "#111827"
C_DIM          = "#6b7280"
C_GRAY         = "#e5e7eb"
C_GREEN        = "#22c55e"
C_GREEN_BG     = "#dcfce7"
C_GREEN_TEXT   = "#166534"
C_RED          = "#ef4444"
C_RED_BG       = "#fee2e2"
C_RED_TEXT     = "#991b1b"
C_ORANGE_BG    = "#fff7ed"
C_ORANGE_TEXT  = "#92400e"
C_AMBER        = "#f59e0b"

ctk.set_appearance_mode("light")
ctk.set_default_color_theme("blue")


# ══════════════════════════════════════════════════════════════
class WSManager:
    def __init__(self, app):
        self.app = app
        self.ws = None
        self.loop = None
        self.connected = False

    def connect(self, ip):
        if self.connected: return
        self.loop = asyncio.new_event_loop()
        self.uri = f"ws://{ip}:{WS_PORT}"
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_until_complete(self._connect())

    async def _connect(self):
        try:
            async with websockets.connect(self.uri, ping_interval=None) as ws:
                self.ws = ws
                self.connected = True
                self.app.on_ws_connected()
                async for msg in ws:
                    self.app.on_ws_message(json.loads(msg))
        except Exception as e:
            self.connected = False
            self.app.on_ws_disconnected(str(e))

    def send(self, data: dict):
        if self.connected and self.loop and self.ws:
            asyncio.run_coroutine_threadsafe(self.ws.send(json.dumps(data)), self.loop)


# ══════════════════════════════════════════════════════════════
class PlayerCard(ctk.CTkFrame):
    def __init__(self, parent, idx, app, **kw):
        super().__init__(parent, fg_color=BG_CARD, border_width=2,
                         border_color=C_GRAY, corner_radius=16, **kw)
        self.idx = idx; self.app = app
        self.enabled = True; self.state = "idle"

        self.num_lbl = ctk.CTkLabel(self, text=str(idx + 1),
            font=ctk.CTkFont(size=38, weight="bold"), text_color=C_ACCENT)
        self.num_lbl.pack(pady=(12, 2))

        self.cv = ctk.CTkCanvas(self, width=68, height=68,
            bg=BG_CARD, highlightthickness=0)
        self.cv.pack(pady=3)
        self.c_outer = self.cv.create_oval(1, 1, 67, 67, fill="#fff", outline="")
        self.c_inner = self.cv.create_oval(8, 8, 60, 60, fill=C_GRAY, outline="")
        self.c_ring  = self.cv.create_oval(0, 0, 68, 68, outline=C_GRAY, width=2)

        self.name_lbl = ctk.CTkLabel(self, text=PLAYER_NAMES[idx],
            font=ctk.CTkFont(size=11, weight="bold"), text_color=C_DIM)
        self.name_lbl.pack()

        self.status_lbl = ctk.CTkLabel(self, text="IDLE",
            font=ctk.CTkFont(size=10), text_color=C_DIM)
        self.status_lbl.pack(pady=(2, 8))

        self.toggle_btn = ctk.CTkButton(self, text="Disable", width=88, height=26,
            fg_color=C_GRAY, hover_color="#d1d5db", text_color=C_TEXT,
            font=ctk.CTkFont(size=10), corner_radius=6, command=self.toggle)
        self.toggle_btn.pack(pady=(0, 12))

    def _apply(self, card_bg, num_col, inner_col, ring_col,
               border_col, status_txt, status_col, name_col=None):
        self.configure(fg_color=card_bg, border_color=border_col)
        self.num_lbl.configure(text_color=num_col)
        self.name_lbl.configure(text_color=name_col or status_col)
        self.status_lbl.configure(text=status_txt, text_color=status_col)
        self.cv.configure(bg=card_bg)
        self.cv.itemconfig(self.c_outer, fill=card_bg)
        self.cv.itemconfig(self.c_inner, fill=inner_col)
        self.cv.itemconfig(self.c_ring,  outline=ring_col)

    def set_idle(self):
        self.state = "idle"
        self._apply(BG_CARD, C_ACCENT, C_GRAY, C_GRAY, C_GRAY, "IDLE", C_DIM, C_DIM)

    def set_ready(self):
        self.state = "ready"
        self._apply(BG_CARD, C_ACCENT, "#fff", C_ACCENT, C_ACCENT, "READY ●", C_ACCENT, C_TEXT)

    def set_winner(self):
        self.state = "winner"
        self._apply(C_ACCENT_LIGHT, C_ACCENT, C_ACCENT, C_ACCENT,
                    C_ACCENT, "BUZZED IN! 🔔", C_ACCENT, C_ACCENT)

    def set_locked(self):
        self.state = "locked"
        self._apply(BG_CARD, C_GRAY, "#d1d5db", C_GRAY, C_GRAY, "LOCKED 🔒", C_DIM, C_DIM)

    def set_correct(self):
        self.state = "correct"
        self._apply(C_GREEN_BG, C_GREEN_TEXT, C_GREEN, C_GREEN,
                    C_GREEN, "CORRECT ✓", C_GREEN_TEXT, C_GREEN_TEXT)

    def set_wrong(self):
        self.state = "wrong"
        self._apply(C_RED_BG, C_RED_TEXT, C_RED, C_RED,
                    C_RED, "WRONG ✗", C_RED_TEXT, C_RED_TEXT)

    def toggle(self):
        if self.enabled:
            self.app.ws.send({"cmd": "lock_player", "player": self.idx})
            self.enabled = False
            self.set_locked()
            self.toggle_btn.configure(text="Enable",
                fg_color=C_ACCENT_LIGHT, text_color=C_ACCENT, hover_color="#e9d5ff")
        else:
            self.app.ws.send({"cmd": "unlock_player", "player": self.idx})
            self.enabled = True
            self.set_idle()
            self.toggle_btn.configure(text="Disable",
                fg_color=C_GRAY, text_color=C_TEXT, hover_color="#d1d5db")

    def reset(self):
        self.toggle_btn.configure(text="Disable",
            fg_color=C_GRAY, text_color=C_TEXT, hover_color="#d1d5db")
        if self.enabled:
            self.set_idle()


# ══════════════════════════════════════════════════════════════
class BuzzerApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("1350x840")
        self.minsize(1100, 720)
        self.configure(fg_color=BG_MAIN)

        self.ws = WSManager(self)

        # Game state
        self.buzz_queue  = []   # ordered list of player idx who pressed
        self.pass_index  = 0   # current position in queue
        self.cards       = []
        self.log_widgets = []

        # Timer
        self.timer_running = False
        self.timer_start   = 0.0
        self.timer_elapsed = 0.0

        # Time limit
        self.limit_job     = None

        self._build_ui()
        self._tick_timer()

    # ─── UI Construction ─────────────────────────────────────────
    def _build_ui(self):
        # ── Header ──────────────────────────────────────────────
        hdr = ctk.CTkFrame(self, fg_color=BG_HEADER, height=64, corner_radius=0)
        hdr.pack(fill="x"); hdr.pack_propagate(False)

        ctk.CTkLabel(hdr, text="🎯  Quiz Buzzer Controller",
            font=ctk.CTkFont(size=22, weight="bold"),
            text_color=C_ACCENT_TEXT).pack(side="left", padx=24, pady=18)

        self.conn_lbl = ctk.CTkLabel(hdr, text="●  Not connected",
            font=ctk.CTkFont(size=13), text_color="#ff8787")
        self.conn_lbl.pack(side="right", padx=24)

        # ── Connection bar ───────────────────────────────────────
        cbar = ctk.CTkFrame(self, fg_color="#f3f4f6", height=46, corner_radius=0)
        cbar.pack(fill="x"); cbar.pack_propagate(False)

        ctk.CTkLabel(cbar, text="ESP32 IP:", text_color=C_TEXT,
            font=ctk.CTkFont(size=12)).pack(side="left", padx=(20, 6), pady=10)
        self.ip_entry = ctk.CTkEntry(cbar, width=155, height=30,
            fg_color="#fff", text_color=C_TEXT)
        self.ip_entry.insert(0, DEFAULT_IP)
        self.ip_entry.pack(side="left", pady=8)
        self.btn_conn = ctk.CTkButton(cbar, text="Connect", width=96, height=30,
            fg_color=C_ACCENT, text_color=C_ACCENT_TEXT,
            hover_color=C_ACCENT_HOVER, command=self.do_connect)
        self.btn_conn.pack(side="left", padx=8, pady=8)

        # ── Timer + Time Limit Strip ─────────────────────────────
        tstrip = ctk.CTkFrame(self, fg_color=BG_MAIN, height=76)
        tstrip.pack(fill="x", padx=18, pady=(12, 0))
        tstrip.pack_propagate(False)

        # Timer display block
        timer_box = ctk.CTkFrame(tstrip, fg_color=BG_HEADER, corner_radius=12)
        timer_box.pack(side="left", padx=(0, 6), pady=4)
        ctk.CTkLabel(timer_box, text="GAME TIMER",
            font=ctk.CTkFont(size=8, weight="bold"),
            text_color="#c084fc").pack(padx=18, pady=(7, 0))
        self.timer_lbl = ctk.CTkLabel(timer_box, text="00:00.000",
            font=ctk.CTkFont(family="Courier New", size=28, weight="bold"),
            text_color="#ffffff")
        self.timer_lbl.pack(padx=18, pady=(0, 7))

        # Stop Timer button
        self.btn_stop_timer = ctk.CTkButton(tstrip, text="⏹  STOP", width=100, height=68,
            fg_color="#374151", hover_color="#4b5563", text_color="#fff",
            font=ctk.CTkFont(size=13, weight="bold"), corner_radius=12,
            command=self.stop_timer)
        self.btn_stop_timer.pack(side="left", padx=(0, 18), pady=4)

        # Divider
        ctk.CTkFrame(tstrip, width=2, fg_color=C_GRAY).pack(
            side="left", fill="y", padx=6, pady=12)

        # Time limit area
        tl_area = ctk.CTkFrame(tstrip, fg_color=BG_MAIN)
        tl_area.pack(side="left", padx=12, pady=4, fill="y")

        ctk.CTkLabel(tl_area, text="TIME LIMIT",
            font=ctk.CTkFont(size=8, weight="bold"),
            text_color=C_DIM).pack(anchor="w")

        tl_row = ctk.CTkFrame(tl_area, fg_color=BG_MAIN)
        tl_row.pack(anchor="w", pady=(4, 0))

        self.limit_switch = ctk.CTkSwitch(tl_row, text="Enable",
            font=ctk.CTkFont(size=12), text_color=C_TEXT,
            progress_color=C_ACCENT, button_color=C_ACCENT,
            command=self._on_toggle_limit)
        self.limit_switch.pack(side="left", padx=(0, 10))

        self.limit_settings = ctk.CTkFrame(tl_row, fg_color=BG_MAIN)
        self.limit_settings.pack(side="left")
        ctk.CTkLabel(self.limit_settings, text="Seconds:",
            font=ctk.CTkFont(size=11), text_color=C_DIM).pack(side="left", padx=(0, 4))
        self.limit_entry = ctk.CTkEntry(self.limit_settings, width=56, height=28,
            fg_color="#fff", text_color=C_TEXT)
        self.limit_entry.insert(0, "60")
        self.limit_entry.pack(side="left")
        self.limit_settings.pack_forget()

        self.countdown_lbl = ctk.CTkLabel(tl_area, text="",
            font=ctk.CTkFont(size=11, weight="bold"), text_color=C_RED)
        self.countdown_lbl.pack(anchor="w", pady=(3, 0))

        # ── Main Buttons ─────────────────────────────────────────
        brow = ctk.CTkFrame(self, fg_color=BG_MAIN)
        brow.pack(fill="x", padx=18, pady=(10, 5))

        btn_cfg = dict(height=58, corner_radius=12,
                       font=ctk.CTkFont(size=16, weight="bold"))

        self.btn_start = ctk.CTkButton(brow, text="▶  START", **btn_cfg,
            fg_color=C_ACCENT, text_color=C_ACCENT_TEXT,
            hover_color=C_ACCENT_HOVER, state="disabled", command=self.cmd_start)
        self.btn_start.pack(side="left", fill="x", expand=True, padx=(0, 4))

        self.btn_correct = ctk.CTkButton(brow, text="✅  CORRECT", **btn_cfg,
            fg_color=C_GREEN_BG, text_color=C_GREEN_TEXT, hover_color="#bbf7d0",
            state="disabled", command=self.cmd_correct)
        self.btn_correct.pack(side="left", fill="x", expand=True, padx=4)

        self.btn_wrong = ctk.CTkButton(brow, text="❌  WRONG", **btn_cfg,
            fg_color=C_RED_BG, text_color=C_RED_TEXT, hover_color="#fecaca",
            state="disabled", command=self.cmd_wrong)
        self.btn_wrong.pack(side="left", fill="x", expand=True, padx=4)

        self.btn_pass = ctk.CTkButton(brow, text="↪  PASS", **btn_cfg,
            fg_color=C_ORANGE_BG, text_color=C_ORANGE_TEXT, hover_color="#fed7aa",
            state="disabled", command=self.cmd_pass)
        self.btn_pass.pack(side="left", fill="x", expand=True, padx=4)

        self.btn_reset = ctk.CTkButton(brow, text="↺  RESET", **btn_cfg,
            fg_color=C_GRAY, text_color=C_TEXT, hover_color="#d1d5db",
            state="disabled", command=self.cmd_reset)
        self.btn_reset.pack(side="left", fill="x", expand=True, padx=(4, 0))

        # ── Banner ───────────────────────────────────────────────
        self.banner = ctk.CTkLabel(self,
            text="Click  Connect  to begin.",
            font=ctk.CTkFont(size=13), fg_color="#f3f4f6",
            corner_radius=10, text_color=C_DIM, height=34)
        self.banner.pack(fill="x", padx=18, pady=(0, 7))

        # ── Content Area (grid + log) ────────────────────────────
        content = ctk.CTkFrame(self, fg_color=BG_MAIN)
        content.pack(fill="both", expand=True, padx=18, pady=(0, 14))

        # Player grid
        grid_f = ctk.CTkFrame(content, fg_color=BG_MAIN)
        grid_f.pack(side="left", fill="both", expand=True)

        for i in range(6):
            r, c = divmod(i, 3)
            card = PlayerCard(grid_f, i, self)
            card.grid(row=r, column=c, padx=5, pady=5, sticky="nsew")
            grid_f.grid_columnconfigure(c, weight=1)
            grid_f.grid_rowconfigure(r, weight=1)
            self.cards.append(card)

        # Activity Log panel
        log_panel = ctk.CTkFrame(content, fg_color=BG_LOG, corner_radius=14,
            width=270, border_width=1, border_color=C_GRAY)
        log_panel.pack(side="right", fill="y", padx=(10, 0))
        log_panel.pack_propagate(False)

        log_hdr = ctk.CTkFrame(log_panel, fg_color=BG_HEADER,
            corner_radius=10, height=44)
        log_hdr.pack(fill="x", padx=8, pady=(8, 0))
        log_hdr.pack_propagate(False)
        ctk.CTkLabel(log_hdr, text="📋  Activity Log",
            font=ctk.CTkFont(size=13, weight="bold"),
            text_color=C_ACCENT_TEXT).pack(pady=12)

        self.log_scroll = ctk.CTkScrollableFrame(log_panel,
            fg_color=BG_LOG, corner_radius=0)
        self.log_scroll.pack(fill="both", expand=True, padx=8, pady=8)

        self.log_empty = ctk.CTkLabel(self.log_scroll,
            text="No activity yet.\nStart a game!", 
            font=ctk.CTkFont(size=12), text_color=C_DIM)
        self.log_empty.pack(pady=24)

    # ─── Timer ───────────────────────────────────────────────────
    def _tick_timer(self):
        if self.timer_running:
            self.timer_elapsed = time.time() - self.timer_start
        ms   = int(self.timer_elapsed * 1000)
        m, s = divmod(ms // 1000, 60)
        self.timer_lbl.configure(text=f"{m:02d}:{s:02d}.{ms % 1000:03d}")
        self.after(33, self._tick_timer)

    def stop_timer(self):
        self.timer_running = False

    def _timer_str(self):
        ms   = int(self.timer_elapsed * 1000)
        m, s = divmod(ms // 1000, 60)
        return f"{m:02d}:{s:02d}.{ms % 1000:03d}"

    # ─── Time Limit ──────────────────────────────────────────────
    def _on_toggle_limit(self):
        if self.limit_switch.get():
            self.limit_settings.pack(side="left")
        else:
            self.limit_settings.pack_forget()
            self.countdown_lbl.configure(text="")

    def _start_countdown(self):
        try:    secs = int(self.limit_entry.get())
        except: secs = 60
        self._countdown_tick(secs)

    def _countdown_tick(self, remaining):
        if not self.limit_switch.get(): return
        if remaining <= 0:
            self.countdown_lbl.configure(text="⏰ Time's Up!", text_color=C_RED)
            self._time_up(); return
        m, s  = divmod(remaining, 60)
        color = C_RED if remaining <= 10 else C_AMBER if remaining <= 30 else C_GREEN_TEXT
        self.countdown_lbl.configure(
            text=f"⏱  {m:02d}:{s:02d} remaining", text_color=color)
        self.limit_job = self.after(1000, lambda: self._countdown_tick(remaining - 1))

    def _time_up(self):
        self.ws.send({"cmd": "time_up"})
        self.stop_timer()
        for card in self.cards:
            if card.enabled: card.set_wrong()
        self._judge_buttons(False)
        self._banner("⏰  Time's Up!  No one answered in time.", C_RED_TEXT)
        self._log_add_special("⏰  Time's Up!")

    # ─── Activity Log ────────────────────────────────────────────
    def _log_add(self, player_idx, timestamp, kind="buzz"):
        self.log_empty.pack_forget()
        order = len(self.log_widgets) + 1
        name  = PLAYER_NAMES[player_idx]

        if   kind == "buzz": badge_col, badge_txt = C_ACCENT, f"#{order}"
        elif kind == "pass": badge_col, badge_txt = C_ORANGE_TEXT, "PASS"
        else:                badge_col, badge_txt = C_RED_TEXT,    "✗"

        entry = ctk.CTkFrame(self.log_scroll, fg_color="#ffffff",
            corner_radius=10, border_width=1, border_color=C_GRAY)
        entry.pack(fill="x", pady=(0, 6))

        top = ctk.CTkFrame(entry, fg_color="#ffffff", corner_radius=0)
        top.pack(fill="x", padx=10, pady=(8, 2))

        ctk.CTkLabel(top, text=badge_txt, width=34, height=18,
            fg_color=badge_col, text_color="#ffffff",
            font=ctk.CTkFont(size=9, weight="bold"),
            corner_radius=4).pack(side="left", padx=(0, 6))
        ctk.CTkLabel(top, text=name,
            font=ctk.CTkFont(size=12, weight="bold"),
            text_color=C_TEXT).pack(side="left")

        ctk.CTkLabel(entry, text=f"⏱  {timestamp}",
            font=ctk.CTkFont(size=11), text_color=C_DIM).pack(
            anchor="w", padx=10, pady=(0, 8))

        self.log_widgets.append(entry)

    def _log_add_special(self, msg):
        self.log_empty.pack_forget()
        entry = ctk.CTkFrame(self.log_scroll, fg_color=C_RED_BG,
            corner_radius=10, border_width=1, border_color=C_RED)
        entry.pack(fill="x", pady=(0, 6))
        ctk.CTkLabel(entry, text=msg,
            font=ctk.CTkFont(size=11, weight="bold"),
            text_color=C_RED_TEXT).pack(padx=10, pady=10)
        self.log_widgets.append(entry)

    def _log_clear(self):
        for w in self.log_widgets: w.destroy()
        self.log_widgets.clear()
        self.log_empty.pack(pady=24)

    # ─── Judge Button Helpers ────────────────────────────────────
    def _judge_buttons(self, enabled: bool):
        s = "normal" if enabled else "disabled"
        self.btn_correct.configure(state=s,
            fg_color=C_GREEN_BG if enabled else "#e5e7eb",
            text_color=C_GREEN_TEXT if enabled else C_DIM)
        self.btn_wrong.configure(state=s,
            fg_color=C_RED_BG if enabled else "#e5e7eb",
            text_color=C_RED_TEXT if enabled else C_DIM)
        self.btn_pass.configure(state=s,
            fg_color=C_ORANGE_BG if enabled else "#e5e7eb",
            text_color=C_ORANGE_TEXT if enabled else C_DIM)

    def _current_player(self):
        if self.pass_index < len(self.buzz_queue):
            return self.buzz_queue[self.pass_index]
        return -1

    # ─── Game Commands ───────────────────────────────────────────
    def cmd_start(self):
        if not self.ws.connected: return
        self.ws.send({"cmd": "start_round"})

        # Reset game queue
        self.buzz_queue = []
        self.pass_index = 0

        # Reset & start timer fresh
        self.timer_elapsed = 0.0
        self.timer_start   = time.time()
        self.timer_running = True

        # Clear log
        self._log_clear()

        # Cancel old countdown if any
        if self.limit_job:
            self.after_cancel(self.limit_job)
            self.limit_job = None
        self.countdown_lbl.configure(text="")

        # Start countdown if enabled
        if self.limit_switch.get():
            self._start_countdown()

        # Cards
        for card in self.cards:
            if card.enabled: card.set_ready()

        # Buttons
        self.btn_start.configure(state="disabled",
            fg_color="#e5e7eb", text_color=C_DIM)
        self._judge_buttons(False)
        self._banner("🎮  Round active — Waiting for first buzz...", C_ACCENT)

    def cmd_correct(self):
        cur = self._current_player()
        if cur < 0: return
        self.ws.send({"cmd": "correct"})
        self.cards[cur].set_correct()
        self.stop_timer()
        if self.limit_job:
            self.after_cancel(self.limit_job); self.limit_job = None
        self._judge_buttons(False)
        self.btn_start.configure(state="normal",
            fg_color=C_ACCENT, text_color=C_ACCENT_TEXT)
        self._banner(f"✅  {PLAYER_NAMES[cur]} answered CORRECTLY!", C_GREEN_TEXT)
        threading.Thread(target=lambda: winsound.Beep(1500, 200), daemon=True).start()

    def cmd_wrong(self):
        cur = self._current_player()
        if cur < 0: return
        self.ws.send({"cmd": "wrong"})
        self.cards[cur].set_wrong()
        self._judge_buttons(False)
        self.btn_start.configure(state="normal",
            fg_color=C_ACCENT, text_color=C_ACCENT_TEXT)
        self._banner(f"❌  {PLAYER_NAMES[cur]} was WRONG.", C_RED_TEXT)
        threading.Thread(target=lambda: winsound.Beep(300, 400), daemon=True).start()

    def cmd_pass(self):
        cur = self._current_player()
        if cur < 0: return

        # Current player → Red
        self.ws.send({"cmd": "pass", "player": cur})
        self.cards[cur].set_wrong()
        self._log_add(cur, self._timer_str(), kind="pass")

        # Advance queue
        self.pass_index += 1

        if self.pass_index >= len(self.buzz_queue):
            # No more players in queue — ask for next press
            self._judge_buttons(False)
            self._banner("↪  Passed. Waiting for next buzz...", C_ORANGE_TEXT)
            return

        # Next player in queue becomes purple
        nxt = self.buzz_queue[self.pass_index]
        self.ws.send({"cmd": "set_winner", "player": nxt})
        self.cards[nxt].set_winner()
        self._judge_buttons(True)
        self._banner(f"↪  Passed to {PLAYER_NAMES[nxt]}!", C_ORANGE_TEXT)

    def cmd_reset(self):
        if not self.ws.connected: return
        self.ws.send({"cmd": "reset"})
        if self.limit_job:
            self.after_cancel(self.limit_job); self.limit_job = None
        self.countdown_lbl.configure(text="")
        self._local_reset()

    def _local_reset(self):
        self.buzz_queue = []
        self.pass_index = 0
        for card in self.cards: card.reset()
        if self.ws.connected:
            self.btn_start.configure(state="normal",
                fg_color=C_ACCENT, text_color=C_ACCENT_TEXT)
            self.btn_reset.configure(state="normal",
                fg_color=C_GRAY, text_color=C_TEXT)
            self._banner("Press  ▶ START  to begin.", C_TEXT)
        else:
            self.btn_start.configure(state="disabled",
                fg_color="#e5e7eb", text_color=C_DIM)
            self.btn_reset.configure(state="disabled",
                fg_color="#e5e7eb", text_color=C_DIM)
            self._banner("Click  Connect  to begin.", C_DIM)
        self._judge_buttons(False)

    def _banner(self, text, color):
        self.banner.configure(text=text, text_color=color)

    # ─── Connection ──────────────────────────────────────────────
    def do_connect(self):
        self.conn_lbl.configure(text="●  Connecting...", text_color="#fcd34d")
        self.ws.connect(self.ip_entry.get().strip() or DEFAULT_IP)

    def on_ws_connected(self):
        self.after(0, lambda: self.conn_lbl.configure(
            text="●  Connected ✓", text_color="#4ade80"))
        self.after(0, lambda: self.btn_conn.configure(
            state="disabled", text="Connected",
            fg_color="#e5e7eb", text_color=C_DIM))
        self.after(0, self._local_reset)

    def on_ws_disconnected(self, err):
        self.after(0, lambda: self.conn_lbl.configure(
            text="●  Disconnected", text_color="#ff8787"))
        self.after(0, lambda: self.btn_conn.configure(
            state="normal", text="Connect",
            fg_color=C_ACCENT, text_color=C_ACCENT_TEXT))
        self.after(0, self._local_reset)

    def on_ws_message(self, data: dict):
        self.after(0, lambda d=data: self._handle(d))

    def _handle(self, data: dict):
        event  = data.get("event", "")
        player = data.get("player", -1)

        if event == "buzzed_in" and 0 <= player <= 5:
            # Add to queue regardless of position
            if player not in self.buzz_queue:
                self.buzz_queue.append(player)
                self._log_add(player, self._timer_str(), kind="buzz")

            # Only update UI if this is the FIRST buzz (current winner)
            if len(self.buzz_queue) == 1:
                threading.Thread(target=lambda: winsound.Beep(2500, 800),
                    daemon=True).start()
                self.cards[player].set_winner()
                for i, card in enumerate(self.cards):
                    if i != player and card.state == "ready":
                        card.set_locked()
                self.btn_start.configure(state="normal",
                    fg_color=C_ACCENT, text_color=C_ACCENT_TEXT)
                self._judge_buttons(True)
                self._banner(f"🔔  {PLAYER_NAMES[player]} buzzed in first!", C_ACCENT)

        elif event == "status":
            locked = data.get("locked", [False] * 6)
            for i, card in enumerate(self.cards):
                if locked[i]: card.set_locked()


if __name__ == "__main__":
    app = BuzzerApp()
    app.mainloop()
