#!/usr/bin/env python3
# ============================================================
# K++ BOOTSTRAP STUDIO
# ============================================================
#
# IDE mínima y robusta para trabajar sobre el compilador K++.
#
# Flujo:
#
#       EDITOR
#          |
#          v
#        K++ KPP
#          |
#          +----------------------+
#          |                      |
#          v                      v
#      RAW / BIN             DISASSEMBLY
#                                 |
#                                 v
#                         payload i386 32-bit
#
# OUTPUT:
#
#   TAB 1 -> K++ OUTPUT
#            stdout + stderr de kpp
#
#   TAB 2 -> DISASSEMBLY
#            BIN - 36 bytes
#            objdump i386 Intel
#
# RUN:
#
#   SOURCE
#      |
#      v
#   K++ compile
#      |
#      v
#   RAW/BIN
#      |
#      v
#   DD skip=36
#      |
#      v
#   OBJDUMP i386 Intel
#
# Linux / Debian / MX Linux
# Python 3 + Tkinter
#
# ============================================================

import os
import subprocess
import threading
import queue
import tempfile

from pathlib import Path

import tkinter as tk
from tkinter import ttk
from tkinter import filedialog
from tkinter import messagebox


# ============================================================
# CONFIGURACIÓN
# ============================================================

APP_NAME = "K++ Bootstrap Studio"

KPP_ROOT = Path(
    "/home/saykol/C/KPP"
)

KPP_BIN = (
    KPP_ROOT
    / "bin"
    / "kpp"
)

PROJECTS_DIR = (
    KPP_ROOT
    / "projects"
)

OUTPUT_DIR = (
    KPP_ROOT
    / "Output"
)

DEFAULT_SOURCE = """\
int main() {
    int x = 10;
    return x;
}
"""

DEFAULT_FILE = (
    KPP_ROOT
    / "bootstrap.kpp"
)

# ------------------------------------------------------------
# CABECERA BINARIA K++
#
# El formato actual contiene 36 bytes de cabecera.
#
# Payload:
#
#     BIN[36:]
#
# ------------------------------------------------------------

KPP_BIN_HEADER_SIZE = 36


# ============================================================
# COLORES
# ============================================================

BLUE_DARK = "#071A33"
BLUE = "#0B3D91"
BLUE_LIGHT = "#1456B8"
BLUE_BUTTON = "#0E4FAF"
BLUE_HOVER = "#1768D1"

BG = "#07111F"
PANEL = "#0B1728"
EDITOR_BG = "#081321"

TEXT = "#D9E7FF"
TEXT_DIM = "#8CA6C8"

GREEN = "#43D17A"
RED = "#FF5C68"
YELLOW = "#FFD166"
CYAN = "#55C7FF"
WHITE = "#FFFFFF"

OUTPUT_BG = "#050D17"


# ============================================================
# APLICACIÓN
# ============================================================

class KPPBootstrapStudio(tk.Tk):

    def __init__(self):

        super().__init__()

        self.title(
            APP_NAME
        )

        self.geometry(
            "1400x850"
        )

        self.minsize(
            1000,
            650
        )

        self.configure(
            bg=BG
        )

        # ----------------------------------------------------
        # ARCHIVO ACTUAL
        # ----------------------------------------------------

        self.current_file = DEFAULT_FILE

        # ----------------------------------------------------
        # PROCESO PRINCIPAL K++
        # ----------------------------------------------------

        self.process = None

        # ----------------------------------------------------
        # COLA DE COMUNICACIÓN
        # ----------------------------------------------------

        self.output_queue = queue.Queue()

        # ----------------------------------------------------
        # ESTADO RUN
        #
        # False:
        #     COMPILAR -> solamente compilar
        #
        # True:
        #     RUN -> compilar + desensamblar
        #
        # ----------------------------------------------------

        self.run_after_compile = False

        # ----------------------------------------------------
        # PROCESO DE DESENSAMBLADO
        # ----------------------------------------------------

        self.disassembly_process = None

        # ====================================================
        # DISASSEMBLY VIEWS
        #
        # CODE = salida completa de objdump
        # DATA = strings/datos legibles del payload
        # RAW  = bytes reales del payload
        # ====================================================

        self.disassembly_view = "CODE"

        self.disassembly_code_lines = []
        self.disassembly_data_text = ""
        self.disassembly_raw_text = ""

        self.disassembly_bin_file = ""
        self.disassembly_payload_size = 0

        # ----------------------------------------------------
        # FUENTES
        # ----------------------------------------------------

        self.font_editor = (
            "DejaVu Sans Mono",
            12
        )

        self.font_console = (
            "DejaVu Sans Mono",
            10
        )

        self.font_disassembly = (
            "DejaVu Sans Mono",
            10
        )

        # ----------------------------------------------------
        # CONSTRUIR GUI
        # ----------------------------------------------------

        self._configure_style()

        self._build_interface()

        self._load_initial_source()

        # ----------------------------------------------------
        # POLLING
        # ----------------------------------------------------

        self.after(
            100,
            self._poll_process_output
        )

    # ========================================================
    # ESTILO
    # ========================================================

    def _configure_style(self):

        style = ttk.Style(
            self
        )

        try:

            style.theme_use(
                "clam"
            )

        except tk.TclError:

            pass

        style.configure(
            "Main.TFrame",
            background=BG
        )

        style.configure(
            "Panel.TFrame",
            background=PANEL
        )

        style.configure(
            "Blue.TButton",
            background=BLUE_BUTTON,
            foreground=WHITE,
            borderwidth=0,
            padding=(14, 8),
            font=(
                "DejaVu Sans",
                10,
                "bold"
            )
        )

        style.map(
            "Blue.TButton",
            background=[
                (
                    "active",
                    BLUE_HOVER
                ),
                (
                    "pressed",
                    BLUE_DARK
                )
            ],
            foreground=[
                (
                    "disabled",
                    "#6680A0"
                ),
                (
                    "!disabled",
                    WHITE
                )
            ]
        )

        # ----------------------------------------------------
        # NOTEBOOK
        # ----------------------------------------------------

        style.configure(
            "Output.TNotebook",
            background=OUTPUT_BG,
            borderwidth=0,
            tabmargins=0
        )

        style.configure(
            "Output.TNotebook.Tab",
            background=PANEL,
            foreground=TEXT,
            padding=(18, 7),
            borderwidth=0,
            font=(
                "DejaVu Sans",
                9,
                "bold"
            )
        )

        style.map(
            "Output.TNotebook.Tab",
            background=[
                (
                    "selected",
                    BLUE
                )
            ],
            foreground=[
                (
                    "selected",
                    WHITE
                ),
                (
                    "!selected",
                    TEXT
                )
            ]
        )

        # ----------------------------------------------------
        # TABS GENERALES
        # ----------------------------------------------------

        style.configure(
            "Tab.TNotebook",
            background=BG,
            borderwidth=0
        )

        style.configure(
            "Tab.TNotebook.Tab",
            background=PANEL,
            foreground=TEXT,
            padding=(15, 8)
        )

        style.map(
            "Tab.TNotebook.Tab",
            background=[
                (
                    "selected",
                    BLUE
                )
            ],
            foreground=[
                (
                    "selected",
                    WHITE
                )
            ]
        )

    # ========================================================
    # INTERFAZ
    # ========================================================

    def _build_interface(self):

        # ====================================================
        # BARRA SUPERIOR
        # ====================================================

        top = tk.Frame(
            self,
            bg=BLUE_DARK,
            height=72
        )

        top.pack(
            side=tk.TOP,
            fill=tk.X
        )

        top.pack_propagate(
            False
        )

        # ----------------------------------------------------
        # MARTILLO
        # ----------------------------------------------------

        hammer = tk.Label(
            top,
            text="🔨",
            bg=BLUE_DARK,
            fg=WHITE,
            font=(
                "DejaVu Sans",
                26
            )
        )

        hammer.pack(
            side=tk.LEFT,
            padx=(18, 8)
        )

        # ----------------------------------------------------
        # K++
        # ----------------------------------------------------

        title = tk.Label(
            top,
            text="K++",
            bg=BLUE_DARK,
            fg=WHITE,
            font=(
                "DejaVu Sans",
                20,
                "bold"
            )
        )

        title.pack(
            side=tk.LEFT
        )

        # ----------------------------------------------------
        # SUBTÍTULO
        # ----------------------------------------------------

        subtitle = tk.Label(
            top,
            text="BOOTSTRAP STUDIO",
            bg=BLUE_DARK,
            fg=CYAN,
            font=(
                "DejaVu Sans",
                10,
                "bold"
            )
        )

        subtitle.pack(
            side=tk.LEFT,
            padx=(10, 20)
        )

        # ----------------------------------------------------
        # COMPILAR
        # ----------------------------------------------------

        self._make_button(
            top,
            "🔨  COMPILAR",
            self.compile_program
        ).pack(
            side=tk.LEFT,
            padx=4,
            pady=14
        )

        # ----------------------------------------------------
        # RUN
        # ----------------------------------------------------

        self._make_button(
            top,
            "▶  RUN",
            self.run_program
        ).pack(
            side=tk.LEFT,
            padx=4,
            pady=14
        )

        # ----------------------------------------------------
        # DEBUG
        # ----------------------------------------------------

        self._make_button(
            top,
            "🐞  DEBUG",
            self.debug_program
        ).pack(
            side=tk.LEFT,
            padx=4,
            pady=14
        )

        # ----------------------------------------------------
        # GUARDAR
        # ----------------------------------------------------

        self._make_button(
            top,
            "💾  GUARDAR",
            self.save_source
        ).pack(
            side=tk.LEFT,
            padx=4,
            pady=14
        )

        # ----------------------------------------------------
        # ABRIR
        # ----------------------------------------------------

        self._make_button(
            top,
            "📂  ABRIR",
            self.open_source
        ).pack(
            side=tk.LEFT,
            padx=4,
            pady=14
        )

        # ----------------------------------------------------
        # ESTADO
        # ----------------------------------------------------

        self.status_label = tk.Label(
            top,
            text="LISTO",
            bg=BLUE_DARK,
            fg=GREEN,
            font=(
                "DejaVu Sans",
                10,
                "bold"
            )
        )

        self.status_label.pack(
            side=tk.RIGHT,
            padx=20
        )

        # ====================================================
        # BARRA DE ARCHIVO
        # ====================================================

        filebar = tk.Frame(
            self,
            bg=PANEL,
            height=34
        )

        filebar.pack(
            side=tk.TOP,
            fill=tk.X
        )

        filebar.pack_propagate(
            False
        )

        tk.Label(
            filebar,
            text="  FUENTE:",
            bg=PANEL,
            fg=TEXT_DIM,
            font=(
                "DejaVu Sans",
                9,
                "bold"
            )
        ).pack(
            side=tk.LEFT
        )

        self.file_label = tk.Label(
            filebar,
            text=str(
                self.current_file
            ),
            bg=PANEL,
            fg=CYAN,
            font=(
                "DejaVu Sans Mono",
                9
            )
        )

        self.file_label.pack(
            side=tk.LEFT,
            padx=10
        )

        # ====================================================
        # ÁREA CENTRAL
        # ====================================================

        center = tk.PanedWindow(
            self,
            orient=tk.HORIZONTAL,
            bg=BLUE_DARK,
            sashwidth=5,
            bd=0
        )

        center.pack(
            fill=tk.BOTH,
            expand=True
        )

        self.after(
            50,
            lambda: self._set_center_split(
                center
            )
        )

        # ====================================================
        # EDITOR
        # ====================================================

        editor_frame = tk.Frame(
            center,
            bg=EDITOR_BG
        )

        center.add(
            editor_frame,
            minsize=400
        )

        editor_header = tk.Frame(
            editor_frame,
            bg=PANEL,
            height=36
        )

        editor_header.pack(
            fill=tk.X
        )

        editor_header.pack_propagate(
            False
        )

        tk.Label(
            editor_header,
            text="  K++ SOURCE",
            bg=PANEL,
            fg=TEXT,
            font=(
                "DejaVu Sans",
                10,
                "bold"
            )
        ).pack(
            side=tk.LEFT,
            pady=8
        )

        # ----------------------------------------------------
        # EDITOR CONTAINER
        # ----------------------------------------------------

        editor_container = tk.Frame(
            editor_frame,
            bg=EDITOR_BG
        )

        editor_container.pack(
            fill=tk.BOTH,
            expand=True
        )

        # ----------------------------------------------------
        # NÚMEROS
        # ----------------------------------------------------

        self.line_numbers = tk.Text(
            editor_container,
            width=5,
            bg="#0A1A2C",
            fg="#54769F",
            bd=0,
            highlightthickness=0,
            font=self.font_editor,
            padx=8,
            pady=10,
            state=tk.DISABLED,
            wrap=tk.NONE
        )

        self.line_numbers.pack(
            side=tk.LEFT,
            fill=tk.Y
        )

        # ----------------------------------------------------
        # EDITOR
        # ----------------------------------------------------

        self.editor = tk.Text(
            editor_container,
            bg=EDITOR_BG,
            fg=TEXT,
            insertbackground=WHITE,
            selectbackground=BLUE,
            selectforeground=WHITE,
            bd=0,
            highlightthickness=0,
            font=self.font_editor,
            padx=12,
            pady=10,
            undo=True,
            wrap=tk.NONE
        )

        self.editor.pack(
            side=tk.LEFT,
            fill=tk.BOTH,
            expand=True
        )

        # ----------------------------------------------------
        # SCROLL VERTICAL
        # ----------------------------------------------------

        editor_scroll_y = ttk.Scrollbar(
            editor_container,
            orient=tk.VERTICAL,
            command=self._editor_yview
        )

        editor_scroll_y.pack(
            side=tk.RIGHT,
            fill=tk.Y
        )

        self.editor.configure(
            yscrollcommand=self._editor_scroll
        )

        # ----------------------------------------------------
        # SCROLL HORIZONTAL
        # ----------------------------------------------------

        editor_scroll_x = ttk.Scrollbar(
            editor_frame,
            orient=tk.HORIZONTAL,
            command=self.editor.xview
        )

        editor_scroll_x.pack(
            side=tk.BOTTOM,
            fill=tk.X
        )

        self.editor.configure(
            xscrollcommand=editor_scroll_x.set
        )

        # ----------------------------------------------------
        # EVENTOS
        # ----------------------------------------------------

        self.editor.bind(
            "<KeyRelease>",
            self._editor_changed
        )

        self.editor.bind(
            "<MouseWheel>",
            self._editor_changed
        )

        self.editor.bind(
            "<Control-s>",
            lambda event: self.save_source()
        )

        self.editor.bind(
            "<F5>",
            lambda event: self.compile_program()
        )

        # ====================================================
        # OUTPUT FRAME
        # ====================================================

        output_frame = tk.Frame(
            center,
            bg=OUTPUT_BG
        )

        center.add(
            output_frame,
            minsize=400
        )

        # ====================================================
        # NOTEBOOK OUTPUT
        # ====================================================

        self.output_notebook = ttk.Notebook(
            output_frame,
            style="Output.TNotebook"
        )

        self.output_notebook.pack(
            fill=tk.BOTH,
            expand=True
        )

        # ====================================================
        # TAB 1
        # K++ OUTPUT
        # ====================================================

        output_tab = tk.Frame(
            self.output_notebook,
            bg=OUTPUT_BG
        )

        self.output_notebook.add(
            output_tab,
            text="  K++ OUTPUT  "
        )

        # ----------------------------------------------------
        # HEADER
        # ----------------------------------------------------

        output_header = tk.Frame(
            output_tab,
            bg=PANEL,
            height=36
        )

        output_header.pack(
            fill=tk.X
        )

        output_header.pack_propagate(
            False
        )

        tk.Label(
            output_header,
            text="  SALIDA DEL COMPILADOR K++",
            bg=PANEL,
            fg=TEXT,
            font=(
                "DejaVu Sans",
                9,
                "bold"
            )
        ).pack(
            side=tk.LEFT,
            pady=8
        )

        # ----------------------------------------------------
        # CLEAR
        # ----------------------------------------------------

        self._make_small_button(
            output_header,
            "CLEAR",
            self.clear_output
        ).pack(
            side=tk.RIGHT,
            padx=(3, 8),
            pady=5
        )

        # ----------------------------------------------------
        # COPY
        # ----------------------------------------------------

        self._make_small_button(
            output_header,
            "COPY",
            self.copy_output
        ).pack(
            side=tk.RIGHT,
            padx=3,
            pady=5
        )

        # ----------------------------------------------------
        # SELECT
        # ----------------------------------------------------

        self._make_small_button(
            output_header,
            "SELECT",
            self.select_output
        ).pack(
            side=tk.RIGHT,
            padx=3,
            pady=5
        )

        # ----------------------------------------------------
        # CONTAINER
        # ----------------------------------------------------

        output_container = tk.Frame(
            output_tab,
            bg=OUTPUT_BG
        )

        output_container.pack(
            fill=tk.BOTH,
            expand=True
        )

        # ----------------------------------------------------
        # TEXTO OUTPUT
        # ----------------------------------------------------

        self.output_text = tk.Text(
            output_container,
            bg=OUTPUT_BG,
            fg="#AFC8E8",
            insertbackground=WHITE,
            selectbackground=BLUE,
            selectforeground=WHITE,
            bd=0,
            highlightthickness=0,
            font=self.font_console,
            padx=12,
            pady=10,
            wrap=tk.NONE,
            undo=False
        )

        self.output_text.pack(
            side=tk.LEFT,
            fill=tk.BOTH,
            expand=True
        )

        # ----------------------------------------------------
        # SCROLL Y
        # ----------------------------------------------------

        output_scroll_y = ttk.Scrollbar(
            output_container,
            orient=tk.VERTICAL,
            command=self.output_text.yview
        )

        output_scroll_y.pack(
            side=tk.RIGHT,
            fill=tk.Y
        )

        self.output_text.configure(
            yscrollcommand=output_scroll_y.set
        )

        # ----------------------------------------------------
        # SCROLL X
        # ----------------------------------------------------

        output_scroll_x = ttk.Scrollbar(
            output_tab,
            orient=tk.HORIZONTAL,
            command=self.output_text.xview
        )

        output_scroll_x.pack(
            side=tk.BOTTOM,
            fill=tk.X
        )

        self.output_text.configure(
            xscrollcommand=output_scroll_x.set
        )

        # ----------------------------------------------------
        # TAGS
        # ----------------------------------------------------

        self.output_text.tag_configure(
            "ok",
            foreground=GREEN
        )

        self.output_text.tag_configure(
            "error",
            foreground=RED
        )

        self.output_text.tag_configure(
            "info",
            foreground=CYAN
        )

        self.output_text.tag_configure(
            "warning",
            foreground=YELLOW
        )

        # ====================================================
        # TAB 2
        # DISASSEMBLY
        # ====================================================

        self.disassembly_tab = tk.Frame(
            self.output_notebook,
            bg=OUTPUT_BG
        )

        self.output_notebook.add(
            self.disassembly_tab,
            text="  DISASSEMBLY  "
        )

        # ----------------------------------------------------
        # HEADER DISASSEMBLY
        # ----------------------------------------------------

        disassembly_header = tk.Frame(
            self.disassembly_tab,
            bg=PANEL,
            height=70
        )

        disassembly_header.pack(
            fill=tk.X
        )

        disassembly_header.pack_propagate(
            False
        )

        # ----------------------------------------------------
        # TITULO
        # ----------------------------------------------------

        tk.Label(
            disassembly_header,
            text="  K++ DISASSEMBLY",
            bg=PANEL,
            fg=TEXT,
            font=(
                "DejaVu Sans",
                10,
                "bold"
            )
        ).pack(
            side=tk.LEFT,
            padx=(4, 18),
            pady=8
        )

        # ----------------------------------------------------
        # INFORMACION BIN
        # ----------------------------------------------------

        self.disassembly_file_label = tk.Label(
            disassembly_header,
            text="Archivo: --",
            bg=PANEL,
            fg=TEXT_DIM,
            font=(
                "DejaVu Sans Mono",
                8
            ),
            anchor="w"
        )

        self.disassembly_file_label.pack(
            side=tk.LEFT,
            padx=4,
            pady=5
        )

        self.disassembly_cpu_label = tk.Label(
            disassembly_header,
            text="CPU: i386",
            bg=PANEL,
            fg=TEXT_DIM,
            font=(
                "DejaVu Sans Mono",
                8
            ),
            anchor="w"
        )

        self.disassembly_cpu_label.pack(
            side=tk.LEFT,
            padx=12,
            pady=5
        )

        self.disassembly_syntax_label = tk.Label(
            disassembly_header,
            text="Syntax: Intel",
            bg=PANEL,
            fg=TEXT_DIM,
            font=(
                "DejaVu Sans Mono",
                8
            ),
            anchor="w"
        )

        self.disassembly_syntax_label.pack(
            side=tk.LEFT,
            padx=4,
            pady=5
        )

        # ----------------------------------------------------
        # ACCIONES
        # ----------------------------------------------------

        self._make_small_button(
            disassembly_header,
            "CLEAR",
            self.clear_disassembly
        ).pack(
            side=tk.RIGHT,
            padx=(3, 8),
            pady=5
        )

        self._make_small_button(
            disassembly_header,
            "COPY",
            self.copy_disassembly
        ).pack(
            side=tk.RIGHT,
            padx=3,
            pady=5
        )

        self._make_small_button(
            disassembly_header,
            "SELECT",
            self.select_disassembly
        ).pack(
            side=tk.RIGHT,
            padx=3,
            pady=5
        )

        # ----------------------------------------------------
        # VIEW BAR
        # ----------------------------------------------------

        disassembly_view_bar = tk.Frame(
            self.disassembly_tab,
            bg=PANEL,
            height=34
        )

        disassembly_view_bar.pack(
            fill=tk.X
        )

        disassembly_view_bar.pack_propagate(
            False
        )

        tk.Label(
            disassembly_view_bar,
            text="  VIEW:",
            bg=PANEL,
            fg=TEXT_DIM,
            font=(
                "DejaVu Sans",
                8,
                "bold"
            )
        ).pack(
            side=tk.LEFT,
            padx=(4, 8)
        )

        self.disassembly_code_button = self._make_small_button(
            disassembly_view_bar,
            "CODE",
            lambda: self.set_disassembly_view("CODE")
        )

        self.disassembly_code_button.pack(
            side=tk.LEFT,
            padx=3,
            pady=4
        )

        self.disassembly_data_button = self._make_small_button(
            disassembly_view_bar,
            "DATA",
            lambda: self.set_disassembly_view("DATA")
        )

        self.disassembly_data_button.pack(
            side=tk.LEFT,
            padx=3,
            pady=4
        )

        self.disassembly_raw_button = self._make_small_button(
            disassembly_view_bar,
            "RAW",
            lambda: self.set_disassembly_view("RAW")
        )

        self.disassembly_raw_button.pack(
            side=tk.LEFT,
            padx=3,
            pady=4
        )

        # ----------------------------------------------------
        # CONTAINER PRINCIPAL
        # ----------------------------------------------------

        disassembly_container = tk.Frame(
            self.disassembly_tab,
            bg=OUTPUT_BG
        )

        disassembly_container.pack(
            fill=tk.BOTH,
            expand=True
        )

        # ----------------------------------------------------
        # TEXTO DISASSEMBLY
        # ----------------------------------------------------

        self.disassembly_text = tk.Text(
            disassembly_container,
            bg=OUTPUT_BG,
            fg="#AFC8E8",
            insertbackground=WHITE,
            selectbackground=BLUE,
            selectforeground=WHITE,
            bd=0,
            highlightthickness=0,
            font=self.font_disassembly,
            padx=12,
            pady=10,
            wrap=tk.NONE,
            undo=False
        )

        self.disassembly_text.pack(
            side=tk.LEFT,
            fill=tk.BOTH,
            expand=True
        )

        # ----------------------------------------------------
        # SCROLL Y
        # ----------------------------------------------------

        disassembly_scroll_y = ttk.Scrollbar(
            disassembly_container,
            orient=tk.VERTICAL,
            command=self.disassembly_text.yview
        )

        disassembly_scroll_y.pack(
            side=tk.RIGHT,
            fill=tk.Y
        )

        self.disassembly_text.configure(
            yscrollcommand=disassembly_scroll_y.set
        )

        # ----------------------------------------------------
        # SCROLL X
        # ----------------------------------------------------

        disassembly_scroll_x = ttk.Scrollbar(
            self.disassembly_tab,
            orient=tk.HORIZONTAL,
            command=self.disassembly_text.xview
        )

        disassembly_scroll_x.pack(
            side=tk.BOTTOM,
            fill=tk.X
        )

        self.disassembly_text.configure(
            xscrollcommand=disassembly_scroll_x.set
        )

        # ----------------------------------------------------
        # TAGS DISASSEMBLY
        # ----------------------------------------------------

        self.disassembly_text.tag_configure(
            "info",
            foreground=CYAN
        )

        self.disassembly_text.tag_configure(
            "ok",
            foreground=GREEN
        )

        self.disassembly_text.tag_configure(
            "error",
            foreground=RED
        )

        self.disassembly_text.tag_configure(
            "warning",
            foreground=YELLOW
        )

        # ====================================================
        # CONSOLA DE NOTIFICACIONES
        # ====================================================

        notification_header = tk.Frame(
            self,
            bg=PANEL,
            height=28
        )

        notification_header.pack(
            side=tk.BOTTOM,
            fill=tk.X
        )

        notification_header.pack_propagate(
            False
        )

        tk.Label(
            notification_header,
            text="  SISTEMA / NOTIFICACIONES",
            bg=PANEL,
            fg=TEXT_DIM,
            font=(
                "DejaVu Sans",
                8,
                "bold"
            )
        ).pack(
            side=tk.LEFT,
            pady=5
        )

        self._make_small_button(
            notification_header,
            "CLEAR",
            self.clear_notifications
        ).pack(
            side=tk.RIGHT,
            padx=(3, 8),
            pady=3
        )

        # ----------------------------------------------------
        # CONTAINER NOTIFICACIONES
        # ----------------------------------------------------

        notification_container = tk.Frame(
            self,
            bg=OUTPUT_BG,
            height=86
        )

        notification_container.pack(
            side=tk.BOTTOM,
            fill=tk.X
        )

        notification_container.pack_propagate(
            False
        )

        # ----------------------------------------------------
        # CONSOLA
        # ----------------------------------------------------

        self.console = tk.Text(
            notification_container,
            bg=OUTPUT_BG,
            fg="#AFC8E8",
            insertbackground=WHITE,
            selectbackground=BLUE,
            selectforeground=WHITE,
            bd=0,
            highlightthickness=0,
            font=(
                "DejaVu Sans Mono",
                9
            ),
            padx=12,
            pady=5,
            wrap=tk.NONE,
            undo=False
        )

        self.console.pack(
            fill=tk.BOTH,
            expand=True
        )

        # ----------------------------------------------------
        # TAGS
        # ----------------------------------------------------

        self.console.tag_configure(
            "ok",
            foreground=GREEN
        )

        self.console.tag_configure(
            "error",
            foreground=RED
        )

        self.console.tag_configure(
            "info",
            foreground=CYAN
        )

        self.console.tag_configure(
            "warning",
            foreground=YELLOW
        )

        # ====================================================
        # STATUS BAR
        # ====================================================

        status = tk.Frame(
            self,
            bg=BLUE_DARK,
            height=26
        )

        status.pack(
            side=tk.BOTTOM,
            fill=tk.X
        )

        status.pack_propagate(
            False
        )

        self.position_label = tk.Label(
            status,
            text="Ln 1, Col 1",
            bg=BLUE_DARK,
            fg=TEXT_DIM,
            font=(
                "DejaVu Sans Mono",
                9
            )
        )

        self.position_label.pack(
            side=tk.RIGHT,
            padx=10
        )

    # ========================================================
    # DIVISIÓN CENTRAL
    # ========================================================

    def _set_center_split(
        self,
        center
    ):

        try:

            width = center.winfo_width()

            if width > 800:

                center.sash_place(
                    0,
                    int(width * 0.58),
                    0
                )

        except Exception:

            pass

    # ========================================================
    # BOTONES
    # ========================================================

    def _make_button(
        self,
        parent,
        text,
        command
    ):

        return tk.Button(
            parent,
            text=text,
            command=command,
            bg=BLUE_BUTTON,
            fg=WHITE,
            activebackground=BLUE_HOVER,
            activeforeground=WHITE,
            relief=tk.FLAT,
            bd=0,
            padx=12,
            pady=7,
            font=(
                "DejaVu Sans",
                9,
                "bold"
            ),
            cursor="hand2"
        )

    # ========================================================

    def _make_small_button(
        self,
        parent,
        text,
        command
    ):

        return tk.Button(
            parent,
            text=text,
            command=command,
            bg=BLUE_BUTTON,
            fg=WHITE,
            activebackground=BLUE_HOVER,
            activeforeground=WHITE,
            relief=tk.FLAT,
            bd=0,
            padx=8,
            pady=3,
            font=(
                "DejaVu Sans",
                8,
                "bold"
            ),
            cursor="hand2"
        )

    # ========================================================
    # OUTPUT
    # ========================================================

    def log_output(
        self,
        message,
        tag="info"
    ):

        self.output_text.insert(
            tk.END,
            message + "\n",
            tag
        )

        self.output_text.see(
            tk.END
        )

    # ========================================================
    # NOTIFICACIONES
    # ========================================================

    def log(
        self,
        message,
        tag="info"
    ):

        self.console.insert(
            tk.END,
            message + "\n",
            tag
        )

        self.console.see(
            tk.END
        )

    # ========================================================
    # OUTPUT - CLEAR
    # ========================================================

    def clear_output(self):

        self.output_text.delete(
            "1.0",
            tk.END
        )

        self.status_label.configure(
            text="LISTO",
            fg=GREEN
        )

    # ========================================================
    # OUTPUT - SELECT
    # ========================================================

    def select_output(self):

        self.output_text.focus_set()

        self.output_text.tag_add(
            tk.SEL,
            "1.0",
            tk.END
        )

        self.output_text.mark_set(
            tk.INSERT,
            "1.0"
        )

        self.output_text.see(
            "1.0"
        )

    # ========================================================
    # OUTPUT - COPY
    # ========================================================

    def copy_output(self):

        try:

            selected = self.output_text.get(
                tk.SEL_FIRST,
                tk.SEL_LAST
            )

        except tk.TclError:

            selected = self.output_text.get(
                "1.0",
                "end-1c"
            )

        if not selected:

            return

        self.clipboard_clear()

        self.clipboard_append(
            selected
        )

        self.update()

    # ========================================================
    # DISASSEMBLY - CAMBIAR VISTA
    # ========================================================

    def set_disassembly_view(self, view):

        if view not in ("CODE", "DATA", "RAW"):
            return

        self.disassembly_view = view

        self.disassembly_text.delete(
            "1.0",
            tk.END
        )

        if view == "CODE":

            content = "\n".join(
                self.disassembly_code_lines
            )

            self.disassembly_text.insert(
                tk.END,
                content
            )

        elif view == "DATA":

            self.disassembly_text.insert(
                tk.END,
                self.disassembly_data_text
            )

        elif view == "RAW":

            self.disassembly_text.insert(
                tk.END,
                self.disassembly_raw_text
            )

        self.disassembly_text.see(
            "1.0"
        )

    # ========================================================
    # DISASSEMBLY - CONSTRUIR DATA
    # ========================================================

    def build_disassembly_data(self, payload):

        lines = []

        printable = []

        start = None

        for index, value in enumerate(payload):

            if 32 <= value <= 126:

                if start is None:
                    start = index

                printable.append(
                    chr(value)
                )

            else:

                if start is not None:

                    if len(printable) >= 4:

                        text = "".join(
                            printable
                        )

                        lines.append(
                            f"{start:08x}  {text}"
                        )

                    printable = []
                    start = None

        if start is not None and len(printable) >= 4:

            text = "".join(
                printable
            )

            lines.append(
                f"{start:08x}  {text}"
            )

        if lines:

            return (
                "============================================================\n"
                "K++ DATA / STRINGS\n"
                "============================================================\n"
                f"PAYLOAD: {len(payload)} bytes\n"
                "OFFSET    STRING\n"
                "------------------------------------------------------------\n"
                + "\n".join(lines)
                + "\n"
            )

        return (
            "============================================================\n"
            "K++ DATA / STRINGS\n"
            "============================================================\n"
            f"PAYLOAD: {len(payload)} bytes\n"
            "No se encontraron cadenas ASCII de 4 bytes o más.\n"
        )

    # ========================================================
    # DISASSEMBLY - CONSTRUIR RAW
    # ========================================================

    def build_disassembly_raw(self, payload):

        lines = []

        for offset in range(
            0,
            len(payload),
            16
        ):

            chunk = payload[
                offset:offset + 16
            ]

            hex_part = " ".join(
                f"{byte:02x}"
                for byte in chunk
            )

            hex_part = (
                hex_part.ljust(47)
            )

            ascii_part = "".join(
                chr(byte)
                if 32 <= byte <= 126
                else "."
                for byte in chunk
            )

            lines.append(
                f"{offset:08x}  {hex_part}  |{ascii_part}|"
            )

        return (
            "============================================================\n"
            "K++ RAW BINARY\n"
            "============================================================\n"
            f"PAYLOAD: {len(payload)} bytes\n"
            "OFFSET    HEX BYTES                                        ASCII\n"
            "------------------------------------------------------------\n"
            + "\n".join(lines)
            + "\n"
        )

    # ========================================================
    # DISASSEMBLY - CLEAR
    # ========================================================

    def clear_disassembly(self):

        self.disassembly_code_lines.clear()

        self.disassembly_data_text = ""
        self.disassembly_raw_text = ""

        self.disassembly_bin_file = ""
        self.disassembly_payload_size = 0

        self.disassembly_text.delete(
            "1.0",
            tk.END
        )

    # ========================================================
    # DISASSEMBLY - SELECT
    # ========================================================

    def select_disassembly(self):

        self.disassembly_text.focus_set()

        self.disassembly_text.tag_add(
            tk.SEL,
            "1.0",
            tk.END
        )

        self.disassembly_text.mark_set(
            tk.INSERT,
            "1.0"
        )

        self.disassembly_text.see(
            "1.0"
        )

    # ========================================================
    # DISASSEMBLY - COPY
    # ========================================================

    def copy_disassembly(self):

        try:

            selected = self.disassembly_text.get(
                tk.SEL_FIRST,
                tk.SEL_LAST
            )

        except tk.TclError:

            selected = self.disassembly_text.get(
                "1.0",
                "end-1c"
            )

        if not selected:

            return

        self.clipboard_clear()

        self.clipboard_append(
            selected
        )

        self.update()

    # ========================================================
    # CLEAR NOTIFICACIONES
    # ========================================================

    def clear_notifications(self):

        self.console.delete(
            "1.0",
            tk.END
        )

    # ========================================================
    # CARGAR FUENTE
    # ========================================================

    def _load_initial_source(self):

        if self.current_file.exists():

            try:

                source = self.current_file.read_text(
                    encoding="utf-8"
                )

                self.editor.delete(
                    "1.0",
                    tk.END
                )

                self.editor.insert(
                    "1.0",
                    source
                )

                self._update_line_numbers()

                self.log(
                    f"[OK] Fuente cargada: {self.current_file}",
                    "ok"
                )

            except Exception as exc:

                self.editor.insert(
                    "1.0",
                    DEFAULT_SOURCE
                )

                self._update_line_numbers()

                self.log(
                    f"[INFO] No se pudo cargar fuente: {exc}",
                    "warning"
                )

        else:

            self.editor.insert(
                "1.0",
                DEFAULT_SOURCE
            )

            self._update_line_numbers()

            self.log(
                "[INFO] Nueva fuente K++ creada.",
                "info"
            )

    # ========================================================
    # LÍNEAS
    # ========================================================

    def _update_line_numbers(self):

        self.line_numbers.configure(
            state=tk.NORMAL
        )

        self.line_numbers.delete(
            "1.0",
            tk.END
        )

        total = int(
            self.editor.index(
                "end-1c"
            ).split(".")[0]
        )

        numbers = "\n".join(
            str(i)
            for i in range(
                1,
                total + 1
            )
        )

        self.line_numbers.insert(
            "1.0",
            numbers
        )

        self.line_numbers.configure(
            state=tk.DISABLED
        )

    # ========================================================
    # SCROLL EDITOR
    # ========================================================

    def _editor_scroll(
        self,
        first,
        last
    ):

        self.line_numbers.yview_moveto(
            first
        )

    # ========================================================

    def _editor_yview(
        self,
        *args
    ):

        self.editor.yview(
            *args
        )

        self.line_numbers.yview_moveto(
            self.editor.yview()[0]
        )

    # ========================================================
    # CAMBIOS EDITOR
    # ========================================================

    def _editor_changed(
        self,
        event=None
    ):

        self._update_line_numbers()

        try:

            line, col = self.editor.index(
                tk.INSERT
            ).split(".")

            self.position_label.configure(
                text=(
                    f"Ln {line}, "
                    f"Col {int(col) + 1}"
                )
            )

        except Exception:

            pass

    # ========================================================
    # GUARDAR
    # ========================================================

    def save_source(self):

        try:

            self.current_file.parent.mkdir(
                parents=True,
                exist_ok=True
            )

            source = self.editor.get(
                "1.0",
                "end-1c"
            )

            self.current_file.write_text(
                source,
                encoding="utf-8"
            )

            self.file_label.configure(
                text=str(
                    self.current_file
                )
            )

            self.log(
                f"[OK] Guardado: {self.current_file}",
                "ok"
            )

            self.status_label.configure(
                text="GUARDADO",
                fg=GREEN
            )

            return True

        except Exception as exc:

            self.log(
                f"[ERROR] Guardando fuente: {exc}",
                "error"
            )

            self.status_label.configure(
                text="ERROR",
                fg=RED
            )

            return False

    # ========================================================
    # ABRIR
    # ========================================================

    def open_source(self):

        filename = filedialog.askopenfilename(
            title="Abrir programa K++",
            initialdir=str(
                PROJECTS_DIR
            ),
            filetypes=[
                (
                    "K++",
                    "*.kpp"
                ),
                (
                    "C++",
                    "*.cpp"
                ),
                (
                    "Todos",
                    "*.*"
                )
            ]
        )

        if not filename:

            return

        try:

            path = Path(
                filename
            )

            source = path.read_text(
                encoding="utf-8"
            )

            self.editor.delete(
                "1.0",
                tk.END
            )

            self.editor.insert(
                "1.0",
                source
            )

            self.current_file = path

            self.file_label.configure(
                text=str(path)
            )

            self._update_line_numbers()

            self.log(
                f"[OK] Abierto: {path}",
                "ok"
            )

        except Exception as exc:

            messagebox.showerror(
                "K++",
                str(exc)
            )

    # ========================================================
    # COMPROBAR K++
    # ========================================================

    def _check_kpp(self):

        if not KPP_BIN.exists():

            self.log(
                f"[ERROR] No existe K++: {KPP_BIN}",
                "error"
            )

            messagebox.showerror(
                "K++ no encontrado",
                (
                    f"No existe:\n\n"
                    f"{KPP_BIN}\n\n"
                    "Construye primero el compilador."
                )
            )

            return False

        if not os.access(
            KPP_BIN,
            os.X_OK
        ):

            self.log(
                "[ERROR] kpp no es ejecutable.",
                "error"
            )

            return False

        return True

    # ========================================================
    # COMPILAR
    # ========================================================

    def compile_program(
        self,
        from_run=False
    ):

        if self.process is not None:

            self.log(
                "[WARNING] Ya existe una compilación en ejecución.",
                "warning"
            )

            return

        if self.disassembly_process is not None:

            self.log(
                "[WARNING] Ya existe un desensamblado en ejecución.",
                "warning"
            )

            return

        if not self._check_kpp():

            return

        if not self.save_source():

            return

        # ----------------------------------------------------
        # ESTADO RUN
        # ----------------------------------------------------

        self.run_after_compile = from_run

        # ----------------------------------------------------
        # LIMPIAR OUTPUT
        # ----------------------------------------------------

        self.output_text.delete(
            "1.0",
            tk.END
        )

        # ----------------------------------------------------
        # SI VIENE DE RUN, LIMPIAR DISASSEMBLY
        # ----------------------------------------------------

        if from_run:

            self.disassembly_text.delete(
                "1.0",
                tk.END
            )

        # ----------------------------------------------------
        # ESTADO
        # ----------------------------------------------------

        self.status_label.configure(
            text="COMPILANDO...",
            fg=YELLOW
        )

        # ----------------------------------------------------
        # NOTIFICACIONES
        # ----------------------------------------------------

        self.log(
            "============================================================",
            "info"
        )

        self.log(
            "K++ BOOTSTRAP",
            "info"
        )

        self.log(
            "============================================================",
            "info"
        )

        self.log(
            f"[SOURCE] {self.current_file}",
            "info"
        )

        self.log(
            f"[KPP]    {KPP_BIN}",
            "info"
        )

        self.log(
            f"[OUTPUT] {OUTPUT_DIR}",
            "info"
        )

        if from_run:

            self.log(
                "[RUN]    compilando y preparando desensamblado...",
                "info"
            )

        else:

            self.log(
                "[RUN]    ejecutando compilador...",
                "info"
            )

        # ----------------------------------------------------
        # WORKER
        # ----------------------------------------------------

        thread = threading.Thread(
            target=self._compile_worker,
            daemon=True
        )

        thread.start()

    # ========================================================
    # WORKER COMPILACIÓN
    # ========================================================

    def _compile_worker(self):

        try:

            command = [
                str(KPP_BIN),
                str(self.current_file)
            ]

            display_command = [
                "./bin/kpp",
                os.path.relpath(
                    self.current_file,
                    KPP_ROOT
                )
            ]

            self.output_queue.put(
                (
                    "output",
                    "[CMD] " + " ".join(
                        display_command
                    )
                )
            )

            self.process = subprocess.Popen(
                command,
                cwd=str(KPP_ROOT),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1
            )

            for line in iter(
                self.process.stdout.readline,
                ""
            ):

                if line:

                    self.output_queue.put(
                        (
                            "output",
                            line.rstrip()
                        )
                    )

            return_code = (
                self.process.wait()
            )

            self.output_queue.put(
                (
                    "returncode",
                    return_code
                )
            )

        except Exception as exc:

            self.output_queue.put(
                (
                    "exception",
                    str(exc)
                )
            )

    # ========================================================
    # POLLING
    # ========================================================

    def _poll_process_output(self):

        try:

            while True:

                kind, value = (
                    self.output_queue.get_nowait()
                )

                # ============================================
                # SALIDA K++
                # ============================================

                if kind == "output":

                    tag = "info"

                    if value.startswith("✔"):

                        tag = "ok"

                    elif (
                        "[ERROR]" in value
                        or "error:" in value.lower()
                        or "fatal" in value.lower()
                    ):

                        tag = "error"

                    elif (
                        "[WARNING]" in value
                        or "warning:" in value.lower()
                    ):

                        tag = "warning"

                    self.log_output(
                        value,
                        tag
                    )

                # ============================================
                # MENSAJE INTERNO
                # ============================================

                elif kind == "info":

                    self.log(
                        value,
                        "info"
                    )

                # ============================================
                # RETURN CODE
                # ============================================

                elif kind == "returncode":

                    self.process = None

                    if value == 0:

                        self._compilation_success()

                    else:

                        self.status_label.configure(
                            text="ERROR",
                            fg=RED
                        )

                        self.log(
                            (
                                "[ERROR] K++ terminó "
                                f"con código {value}"
                            ),
                            "error"
                        )

                        # No ejecutar disassembly
                        # si K++ falló.

                        self.run_after_compile = False

                # ============================================
                # EXCEPCIÓN
                # ============================================

                elif kind == "exception":

                    self.process = None

                    self.status_label.configure(
                        text="ERROR",
                        fg=RED
                    )

                    self.log(
                        f"[ERROR] {value}",
                        "error"
                    )

                    self.run_after_compile = False

                # ============================================
                # DISASSEMBLY OUTPUT
                # ============================================

                elif kind == "disassembly_output":

                    self.disassembly_code_lines.append(
                        value
                    )

                    if self.disassembly_view == "CODE":

                        self.disassembly_text.insert(
                            tk.END,
                            value + "\n",
                            "info"
                        )

                        self.disassembly_text.see(
                            tk.END
                        )

                # ============================================
                # DISASSEMBLY INFO
                # ============================================

                elif kind == "disassembly_info":

                    self.log(
                        value,
                        "info"
                    )

                # ============================================
                # DISASSEMBLY DATA
                # ============================================

                elif kind == "disassembly_data":

                    self.disassembly_data_text = value

                    if self.disassembly_view == "DATA":

                        self.disassembly_text.delete(
                            "1.0",
                            tk.END
                        )

                        self.disassembly_text.insert(
                            tk.END,
                            value
                        )

                # ============================================
                # DISASSEMBLY RAW
                # ============================================

                elif kind == "disassembly_raw":

                    self.disassembly_raw_text = value

                    if self.disassembly_view == "RAW":

                        self.disassembly_text.delete(
                            "1.0",
                            tk.END
                        )

                        self.disassembly_text.insert(
                            tk.END,
                            value
                        )

                # ============================================
                # DISASSEMBLY ERROR
                # ============================================

                elif kind == "disassembly_error":

                    self.log_disassembly(
                        value,
                        "error"
                    )

                # ============================================
                # DISASSEMBLY RETURN
                # ============================================

                elif kind == "disassembly_returncode":

                    self.disassembly_process = None

                    if value == 0:

                        self.status_label.configure(
                            text="RUN OK",
                            fg=GREEN
                        )

                        self.log(
                            "[OK] Desensamblado completado.",
                            "ok"
                        )

                        self.log(
                            "[OK] Payload i386 32-bit generado correctamente.",
                            "ok"
                        )

                    else:

                        self.status_label.configure(
                            text="DISASM ERROR",
                            fg=RED
                        )

                        self.log(
                            (
                                "[ERROR] objdump terminó "
                                f"con código {value}"
                            ),
                            "error"
                        )

                # ============================================
                # DISASSEMBLY EXCEPTION
                # ============================================

                elif kind == "disassembly_exception":

                    self.disassembly_process = None

                    self.status_label.configure(
                        text="DISASM ERROR",
                        fg=RED
                    )

                    self.log(
                        f"[ERROR] Desensamblado: {value}",
                        "error"
                    )

        except queue.Empty:

            pass

        self.after(
            100,
            self._poll_process_output
        )

    # ========================================================
    # COMPILACIÓN CORRECTA
    # ========================================================

    def _compilation_success(self):

        # ----------------------------------------------------
        # NOTIFICACIÓN
        # ----------------------------------------------------

        self.log(
            "------------------------------------------------------------",
            "ok"
        )

        self.log(
            "[OK] K++ terminó correctamente.",
            "ok"
        )

        # ----------------------------------------------------
        # NOMBRE
        # ----------------------------------------------------

        stem = (
            self.current_file.stem
        )

        raw_file = (
            OUTPUT_DIR
            / f"{stem}.raw"
        )

        bin_file = (
            OUTPUT_DIR
            / f"{stem}.bin"
        )

        raw_exists = (
            raw_file.exists()
        )

        bin_exists = (
            bin_file.exists()
        )

        # ====================================================
        # RAW
        # ====================================================

        if raw_exists:

            self.log(
                f"[OK] RAW: {raw_file}",
                "ok"
            )

            self.log(
                (
                    "[OK] Tamaño RAW: "
                    f"{raw_file.stat().st_size} bytes"
                ),
                "ok"
            )

        else:

            self.log(
                f"[WARNING] No apareció RAW: {raw_file}",
                "warning"
            )

        # ====================================================
        # BIN
        # ====================================================

        if bin_exists:

            self.log(
                f"[OK] BIN: {bin_file}",
                "ok"
            )

            self.log(
                (
                    "[OK] Tamaño BIN: "
                    f"{bin_file.stat().st_size} bytes"
                ),
                "ok"
            )

        else:

            self.log(
                f"[ERROR] No apareció BIN: {bin_file}",
                "error"
            )

        # ====================================================
        # CORRECTO
        # ====================================================

        if raw_exists and bin_exists:

            self.status_label.configure(
                text="BOOTSTRAP OK",
                fg=GREEN
            )

            self.log(
                "============================================================",
                "ok"
            )

            self.log(
                "BOOTSTRAP COMPLETADO.",
                "ok"
            )

            self.log(
                f"[SOURCE] {self.current_file.name}",
                "ok"
            )

            self.log(
                f"[BIN]    {bin_file}",
                "ok"
            )

            self.log(
                "============================================================",
                "ok"
            )

            # ------------------------------------------------
            # RUN:
            #
            # COMPILACIÓN -> DISASSEMBLY
            #
            # ------------------------------------------------

            if self.run_after_compile:

                self.log(
                    "[RUN] Iniciando desensamblado automático...",
                    "info"
                )

                self._start_disassembly(
                    bin_file
                )

        else:

            self.status_label.configure(
                text="SALIDA INCOMPLETA",
                fg=YELLOW
            )

            self.log(
                (
                    "[WARNING] El compilador terminó "
                    "correctamente, pero no se encontraron "
                    "todos los artefactos esperados."
                ),
                "warning"
            )

            self.run_after_compile = False

    # ========================================================
    # RUN
    # ========================================================

    def run_program(self):

        if self.process is not None:

            self.log(
                "[WARNING] Ya existe una compilación en ejecución.",
                "warning"
            )

            return

        self.log(
            "[RUN] Ejecutando pipeline K++ completo.",
            "info"
        )

        self.log(
            "[RUN] SOURCE -> KPP -> BIN -> DD -> OBJDUMP.",
            "info"
        )

        self.compile_program(
            from_run=True
        )

    # ========================================================
    # DISASSEMBLY
    # ========================================================

    def _start_disassembly(
        self,
        bin_file
    ):

        if self.disassembly_process is not None:

            self.log(
                "[WARNING] Ya existe un desensamblado en ejecución.",
                "warning"
            )

            return

        if not bin_file.exists():

            self.log(
                f"[ERROR] No existe BIN: {bin_file}",
                "error"
            )

            return

        try:

            size = (
                bin_file.stat().st_size
            )

        except OSError as exc:

            self.log(
                f"[ERROR] No se pudo consultar BIN: {exc}",
                "error"
            )

            return

        # ----------------------------------------------------
        # COMPROBAR TAMAÑO
        # ----------------------------------------------------

        if size <= KPP_BIN_HEADER_SIZE:

            self.log(
                (
                    "[ERROR] BIN demasiado pequeño para "
                    f"extraer {KPP_BIN_HEADER_SIZE} bytes "
                    "de cabecera."
                ),
                "error"
            )

            self.status_label.configure(
                text="BIN INVÁLIDO",
                fg=RED
            )

            return

        # ----------------------------------------------------
        # SELECCIONAR TAB DISASSEMBLY
        # ----------------------------------------------------

        self.output_notebook.select(
            self.disassembly_tab
        )

        # ----------------------------------------------------
        # LIMPIAR
        # ----------------------------------------------------

        self.disassembly_code_lines.clear()

        self.disassembly_data_text = ""
        self.disassembly_raw_text = ""

        self.disassembly_text.delete(
            "1.0",
            tk.END
        )

        # ----------------------------------------------------
        # INFORMACIÓN BIN
        # ----------------------------------------------------

        payload_size = (
            size
            - KPP_BIN_HEADER_SIZE
        )

        self.disassembly_bin_file = str(
            bin_file
        )

        self.disassembly_payload_size = (
            payload_size
        )

        self.disassembly_file_label.configure(
            text=f"Archivo: {bin_file.name}"
        )

        self.disassembly_cpu_label.configure(
            text="CPU: i386"
        )

        self.disassembly_syntax_label.configure(
            text="Syntax: Intel"
        )

        self.log(
            f"[DISASM] BIN: {bin_file}",
            "info"
        )

        self.log(
            (
                "[DISASM] HEADER: "
                f"{KPP_BIN_HEADER_SIZE} bytes"
            ),
            "info"
        )

        self.log(
            (
                "[DISASM] PAYLOAD: "
                f"{payload_size} bytes"
            ),
            "info"
        )

        self.log(
            "[DISASM] FORMAT: raw binary / i386 / Intel",
            "info"
        )

        # ----------------------------------------------------
        # CREAR NOMBRE TEMPORAL
        # ----------------------------------------------------

        safe_stem = (
            bin_file.stem
        )

        temp_file = (
            Path(tempfile.gettempdir())
            / f"kpp_{safe_stem}_payload.bin"
        )

        # ----------------------------------------------------
        # WORKER
        # ----------------------------------------------------

        thread = threading.Thread(
            target=self._disassembly_worker,
            args=(
                bin_file,
                temp_file
            ),
            daemon=True
        )

        thread.start()

    # ========================================================
    # WORKER DISASSEMBLY
    # ========================================================

    def _disassembly_worker(
        self,
        bin_file,
        temp_file
    ):

        try:

            # =================================================
            # DD
            # =================================================

            dd_command = [
                "dd",
                f"if={bin_file}",
                f"of={temp_file}",
                "bs=1",
                f"skip={KPP_BIN_HEADER_SIZE}",
                "status=none"
            ]

            self.output_queue.put(
                (
                    "disassembly_info",
                    "[DD] " + " ".join(
                        dd_command
                    )
                )
            )

            dd_process = subprocess.run(
                dd_command,
                cwd=str(KPP_ROOT),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )

            if dd_process.returncode != 0:

                error = (
                    dd_process.stderr.strip()
                    or "dd terminó con error."
                )

                self.output_queue.put(
                    (
                        "disassembly_error",
                        f"[ERROR] dd: {error}"
                    )
                )

                self.output_queue.put(
                    (
                        "disassembly_returncode",
                        dd_process.returncode
                    )
                )

                return

            # =================================================
            # VERIFICAR PAYLOAD
            # =================================================

            if not temp_file.exists():

                self.output_queue.put(
                    (
                        "disassembly_error",
                        "[ERROR] dd no creó el payload."
                    )
                )

                self.output_queue.put(
                    (
                        "disassembly_returncode",
                        1
                    )
                )

                return

            payload_size = (
                temp_file.stat().st_size
            )

            self.output_queue.put(
                (
                    "disassembly_info",
                    (
                        "[OK] Payload extraído: "
                        f"{payload_size} bytes"
                    )
                )
            )

            # =================================================
            # PREPARAR DATA Y RAW
            # =================================================

            with open(
                temp_file,
                "rb"
            ) as payload_stream:

                payload = payload_stream.read()

            data_text = self.build_disassembly_data(
                payload
            )

            raw_text = self.build_disassembly_raw(
                payload
            )

            self.output_queue.put(
                (
                    "disassembly_data",
                    data_text
                )
            )

            self.output_queue.put(
                (
                    "disassembly_raw",
                    raw_text
                )
            )

            # =================================================
            # OBJDUMP
            # =================================================

            objdump_command = [
                "objdump",
                "-D",
                "-b",
                "binary",
                "-m",
                "i386",
                "-Mintel",
                str(temp_file)
            ]

            self.output_queue.put(
                (
                    "disassembly_info",
                    "[OBJDUMP] " + " ".join(
                        objdump_command
                    )
                )
            )

            process = subprocess.Popen(
                objdump_command,
                cwd=str(KPP_ROOT),
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1
            )

            self.disassembly_process = process

            self.output_queue.put(
                (
                    "disassembly_info",
                    "[OK] objdump iniciado correctamente."
                )
            )

            for line in iter(
                process.stdout.readline,
                ""
            ):

                if line:

                    self.output_queue.put(
                        (
                            "disassembly_output",
                            line.rstrip()
                        )
                    )

            return_code = (
                process.wait()
            )

            self.output_queue.put(
                (
                    "disassembly_returncode",
                    return_code
                )
            )

            # El proceso ya terminó. Liberar la referencia
            # para que RUN/STOP no conserve un Popen antiguo.
            self.disassembly_process = None

            # =================================================
            # LIMPIAR TEMPORAL
            # =================================================

            try:

                temp_file.unlink(
                    missing_ok=True
                )

            except Exception:

                pass

        except FileNotFoundError as exc:

            self.disassembly_process = None

            self.output_queue.put(
                (
                    "disassembly_exception",
                    (
                        "No se encontró una herramienta "
                        f"necesaria: {exc}"
                    )
                )
            )

        except Exception as exc:

            self.disassembly_process = None

            self.output_queue.put(
                (
                    "disassembly_exception",
                    str(exc)
                )
            )

    # ========================================================
    # LOG DISASSEMBLY
    # ========================================================

    def log_disassembly(
        self,
        message,
        tag="info"
    ):

        self.disassembly_text.insert(
            tk.END,
            message + "\n",
            tag
        )

        self.disassembly_text.see(
            tk.END
        )

    # ========================================================
    # DEBUG
    # ========================================================

    def debug_program(self):

        self.log(
            "[DEBUG] El depurador se añadirá después del bootstrap.",
            "warning"
        )

        self.log(
            "[DEBUG] Por ahora verificamos SOURCE -> KPP -> RAW/BIN.",
            "info"
        )

    # ========================================================
    # CERRAR
    # ========================================================

    def destroy(self):

        # ----------------------------------------------------
        # K++
        # ----------------------------------------------------

        if self.process is not None:

            try:

                self.process.terminate()

            except Exception:

                pass

        # ----------------------------------------------------
        # OBJDUMP
        # ----------------------------------------------------

        if self.disassembly_process is not None:

            try:

                self.disassembly_process.terminate()

            except Exception:

                pass

        super().destroy()


# ============================================================
# MAIN
# ============================================================

def main():

    app = KPPBootstrapStudio()

    app.mainloop()


# ============================================================
# ENTRY POINT
# ============================================================

if __name__ == "__main__":

    main()
