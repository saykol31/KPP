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
#          |                      |
#          v                      v
#   MOVER BINARIO         payload i386 32-bit
#          |
#          v
#   imagen persistente
#   de KolibriOS
#   (AUTORUN.DAT)
#          |
#          v
#   EJECUTAR -> QEMU
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
import shutil
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

# La raíz del repo sale de la ubicación de este script: el Studio
# vive en la raíz, así se encuentra solo en cualquier clon
# (MX: ~/C/KPP, Surface: ~/Home/KPP). La variable de entorno
# KPP_ROOT la sobreescribe si hace falta, como ASM_HOME en asm_env.sh.
KPP_ROOT = Path(
    os.environ.get(
        "KPP_ROOT",
        str(Path(__file__).resolve().parent)
    )
)

# El binario del compilador se resuelve por existencia:
# en Windows el build produce kpp.exe y en Linux/Debian,
# kpp a secas. El Studio ejecuta el que se encuentre; si
# no hay ninguno, KPP_BIN queda con el nombre propio de
# la plataforma, para que el error de _check_kpp nombre
# el archivo correcto en cada sistema.

KPP_BIN_WIN = (
    KPP_ROOT
    / "bin"
    / "kpp.exe"
)

KPP_BIN_UNIX = (
    KPP_ROOT
    / "bin"
    / "kpp"
)

if KPP_BIN_WIN.exists():

    KPP_BIN = KPP_BIN_WIN

elif KPP_BIN_UNIX.exists():

    KPP_BIN = KPP_BIN_UNIX

elif os.name == "nt":

    KPP_BIN = KPP_BIN_WIN

else:

    KPP_BIN = KPP_BIN_UNIX

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
    / "compiler"
    / "kpp"
    / "frontend"
    / "Lexer.kpp"
)

# ------------------------------------------------------------
# KOLIBRIOS
#
# Imagen persistente y parámetros de QEMU: los mismos valores
# que la configuración de asm_env.sh (Mini Entorno ASM).
# La ruta de la imagen es configuración de cada máquina, no
# código: se puede sobreescribir con KPP_KOLIBRIOS_IMAGE.
# ------------------------------------------------------------

KOLIBRIOS_IMAGE = Path(
    os.environ.get(
        "KPP_KOLIBRIOS_IMAGE",
        str(
            Path.home()
            / "VirtualBox VMs"
            / "kolibriOS"
            / "KPP-KOLIBRI.img"
        )
    )
)

# Partición FAT32 de la imagen:
# sector inicial 2048 * 512 bytes = 1048576
KOLIBRIOS_OFFSET = 2048 * 512

# Memoria para QEMU
KOLIBRIOS_QEMU_MEM = "256"


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

# ============================================================
# FAT32 MINIMO PARA LA IMAGEN DE KOLIBRIOS.
#
# Lo justo para el ciclo del Studio, sin herramientas
# externas (adios mtools): abrir la particion FAT32 en su
# offset dentro de la imagen, listar el directorio raiz,
# leer un archivo por su ruta y escribirlo (crear o
# reemplazar) asignando clusters y actualizando las FAT.
#
# Solo nombres 8.3, que son los que KolibriOS usa. Las
# entradas de nombre largo (LFN) se respetan al leer: se
# saltan y nunca se pisa una entrada ajena.
#
# Orden de escritura pensado para el peor caso: primero
# los datos en clusters nuevos, despues las FAT y al final
# la entrada del directorio. Si algo se corta a mitad, lo
# peor que queda son clusters huerfanos, nunca un
# directorio apuntando a basura.
# ============================================================

import struct
import time


# ------------------------------------------------------------
# CKSUM POSIX (el del comando cksum de toda la vida).
#
# El Studio lo usa para el sufijo de 2 hex de los nombres
# 8.3 largos (SemanticAnalyzer -> SEMANTA5). Antes se
# llamaba al comando externo; aqui va la misma cuenta en
# Python puro: mismo CRC, mismos nombres, en Windows y en
# Linux.
# ------------------------------------------------------------

_CKSUM_TAB = []

for _i in range(256):

    _crc = _i << 24

    for _ in range(8):

        if _crc & 0x80000000:

            _crc = ((_crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF

        else:

            _crc = (_crc << 1) & 0xFFFFFFFF

    _CKSUM_TAB.append(_crc)


def kolibri_cksum(datos):

    crc = 0

    for b in datos:

        crc = ((crc << 8) & 0xFFFFFFFF) ^ _CKSUM_TAB[
            ((crc >> 24) ^ b) & 0xFF
        ]

    n = len(datos)

    while n:

        crc = ((crc << 8) & 0xFFFFFFFF) ^ _CKSUM_TAB[
            ((crc >> 24) ^ (n & 0xFF)) & 0xFF
        ]

        n >>= 8

    return crc ^ 0xFFFFFFFF


class KolibriFAT32:

    EOC = 0x0FFFFFF8

    def __init__(self, ruta_imagen, offset):

        self.offset = offset

        self.f = open(ruta_imagen, "r+b")

        try:

            self.f.seek(offset)

            bpb = self.f.read(512)

            if len(bpb) < 512:

                raise ValueError("imagen demasiado corta")

            self.bytes_sector = struct.unpack_from(
                "<H", bpb, 11
            )[0]

            self.sectores_cluster = bpb[13]

            reservados = struct.unpack_from(
                "<H", bpb, 14
            )[0]

            self.num_fats = bpb[16]

            total32 = struct.unpack_from(
                "<I", bpb, 32
            )[0]

            fat32 = struct.unpack_from(
                "<I", bpb, 36
            )[0]

            self.cluster_raiz = struct.unpack_from(
                "<I", bpb, 44
            )[0]

            self.sector_fsinfo = struct.unpack_from(
                "<H", bpb, 48
            )[0]

            if (
                self.bytes_sector not in (512, 1024, 2048, 4096)
                or self.sectores_cluster == 0
                or self.num_fats == 0
                or fat32 == 0
                or total32 == 0
            ):

                raise ValueError(
                    "la particion no parece FAT32"
                )

            self.bytes_cluster = (
                self.bytes_sector * self.sectores_cluster
            )

            self.fat_inicio = offset + reservados * self.bytes_sector

            self.fat_bytes = fat32 * self.bytes_sector

            self.datos_inicio = (
                offset
                + (reservados + self.num_fats * fat32)
                * self.bytes_sector
            )

            self.num_clusters = (
                total32 - (reservados + self.num_fats * fat32)
            ) // self.sectores_cluster

        except Exception:

            self.f.close()

            raise

    # --------------------------------------------------------
    # GEOMETRIA
    # --------------------------------------------------------

    def _cluster_offset(self, cluster):

        return (
            self.datos_inicio
            + (cluster - 2) * self.bytes_cluster
        )

    def _fat_leer(self, cluster):

        self.f.seek(self.fat_inicio + cluster * 4)

        return struct.unpack(
            "<I", self.f.read(4)
        )[0] & 0x0FFFFFFF

    def _fat_escribir(self, cluster, valor):

        for i in range(self.num_fats):

            self.f.seek(
                self.fat_inicio
                + i * self.fat_bytes
                + cluster * 4
            )

            self.f.write(struct.pack("<I", valor))

    def _cadena(self, primero):

        cadena = []

        c = primero

        while 2 <= c < self.EOC:

            cadena.append(c)

            c = self._fat_leer(c)

        return cadena

    def _actualizar_fsinfo(self):

        # El sector FSInfo guarda un resumen (clusters
        # libres y siguiente libre) que fsck comprueba.
        # Se recalcula de la FAT entera: son unos cientos
        # de KB y asi el resumen nunca miente.

        if self.sector_fsinfo in (0, 0xFFFF):

            return

        pos = self.offset + self.sector_fsinfo * self.bytes_sector

        self.f.seek(pos)

        info = self.f.read(512)

        if (
            struct.unpack_from("<I", info, 0)[0] != 0x41615252
            or struct.unpack_from("<I", info, 484)[0]
            != 0x61417272
        ):

            return

        self.f.seek(self.fat_inicio)

        fat = self.f.read(self.fat_bytes)

        libres = 0

        siguiente = 0xFFFFFFFF

        for c in range(2, 2 + self.num_clusters):

            valor = struct.unpack_from(
                "<I", fat, c * 4
            )[0] & 0x0FFFFFFF

            if valor == 0:

                libres += 1

                if siguiente == 0xFFFFFFFF:

                    siguiente = c

        self.f.seek(pos + 488)

        self.f.write(struct.pack("<I", libres))

        self.f.write(struct.pack("<I", siguiente))

    def _liberar_cadena(self, primero):

        for c in self._cadena(primero):

            self._fat_escribir(c, 0)

    def _asignar_cadena(self, cantidad):

        if cantidad == 0:

            return []

        cadena = []

        c = 2

        tope = 2 + self.num_clusters

        while len(cadena) < cantidad and c < tope:

            if self._fat_leer(c) == 0:

                cadena.append(c)

            c += 1

        if len(cadena) < cantidad:

            raise ValueError(
                "sin espacio libre en la imagen"
            )

        for i, cl in enumerate(cadena):

            siguiente = (
                cadena[i + 1]
                if i + 1 < len(cadena)
                else 0x0FFFFFFF
            )

            self._fat_escribir(cl, siguiente)

        return cadena

    # --------------------------------------------------------
    # DIRECTORIOS
    # --------------------------------------------------------

    @staticmethod
    def _nombre83(componente):

        if "." in componente:

            nombre, ext = componente.rsplit(".", 1)

        else:

            nombre, ext = componente, ""

        nombre = nombre.upper().ljust(8)[:8]

        ext = ext.upper().ljust(3)[:3]

        return nombre.encode("latin-1") + ext.encode("latin-1")

    @staticmethod
    def _visible(nombre11):

        nombre = nombre11[:8].decode(
            "latin-1"
        ).rstrip()

        ext = nombre11[8:].decode(
            "latin-1"
        ).rstrip()

        return f"{nombre}.{ext}" if ext else nombre

    @staticmethod
    def _lfn_de(trozos):

        if not trozos:

            return None

        trozos.sort(key=lambda t: t[0])

        chars = []

        for _, texto in trozos:

            for ch in texto:

                if ch == "\x00":

                    return "".join(chars)

                if ch != "\uffff":

                    chars.append(ch)

        return "".join(chars) or None

    def _leer_dir(self, clusters):

        entradas = []

        lfn_pendiente = []

        for cl in clusters:

            self.f.seek(self._cluster_offset(cl))

            datos = self.f.read(self.bytes_cluster)

            for i in range(0, len(datos), 32):

                e = datos[i:i + 32]

                if len(e) < 32 or e[0] == 0x00:

                    return entradas

                if e[0] == 0xE5:

                    lfn_pendiente = []

                    continue

                attr = e[11]

                if attr == 0x0F:

                    # Trozo de nombre largo (LFN): se guarda
                    # y se pega a la entrada real que sigue.

                    seq = e[0] & 0x1F

                    crudos = (
                        e[1:11] + e[14:26] + e[28:32]
                    )

                    texto = crudos.decode(
                        "utf-16-le", errors="replace"
                    )

                    lfn_pendiente.append((seq, texto))

                    continue

                primero = (
                    struct.unpack_from("<H", e, 20)[0] << 16
                ) | struct.unpack_from("<H", e, 26)[0]

                lfn = self._lfn_de(lfn_pendiente)

                lfn_pendiente = []

                entradas.append(
                    {
                        "nombre11": bytes(e[:11]),
                        "nombre": lfn or self._visible(e[:11]),
                        "lfn": lfn,
                        "attr": attr,
                        "es_dir": bool(attr & 0x10),
                        "cluster": primero,
                        "size": struct.unpack_from(
                            "<I", e, 28
                        )[0],
                        "pos": self._cluster_offset(cl) + i,
                        "fecha": struct.unpack_from(
                            "<H", e, 24
                        )[0],
                        "hora": struct.unpack_from(
                            "<H", e, 22
                        )[0],
                    }
                )

        return entradas

    def _coincide(self, entrada, componente):

        if entrada["nombre11"] == self._nombre83(componente):

            return True

        return (
            entrada["nombre"].upper() == componente.upper()
        )

    def _dir_de_ruta(self, componentes):

        clusters = self._cadena(self.cluster_raiz)

        for comp in componentes:

            siguiente = None

            for e in self._leer_dir(clusters):

                if self._coincide(e, comp) and e["es_dir"]:

                    siguiente = e["cluster"]

                    break

            if siguiente is None:

                raise FileNotFoundError(
                    f"no existe el directorio: {comp}"
                )

            clusters = self._cadena(siguiente)

        return clusters

    def _buscar(self, ruta):

        partes = [
            p for p in ruta.replace("\\", "/").split("/")
            if p
        ]

        if not partes:

            raise FileNotFoundError("ruta vacia")

        dir_clusters = self._dir_de_ruta(partes[:-1])

        for e in self._leer_dir(dir_clusters):

            if self._coincide(e, partes[-1]) and not e["es_dir"]:

                return dir_clusters, e

        return dir_clusters, None

    # --------------------------------------------------------
    # LECTURA
    # --------------------------------------------------------

    def listar_raiz(self):

        return self._leer_dir(
            self._cadena(self.cluster_raiz)
        )

    def existe(self, ruta):

        try:

            _, e = self._buscar(ruta)

            return e is not None

        except FileNotFoundError:

            return False

    def leer(self, ruta):

        _, e = self._buscar(ruta)

        if e is None:

            raise FileNotFoundError(
                f"no existe el archivo: {ruta}"
            )

        datos = bytearray()

        for cl in self._cadena(e["cluster"]):

            self.f.seek(self._cluster_offset(cl))

            datos += self.f.read(self.bytes_cluster)

        return bytes(datos[:e["size"]])

    # --------------------------------------------------------
    # ESCRITURA
    # --------------------------------------------------------

    @staticmethod
    def _fecha_hora_fat():

        t = time.localtime()

        hora = (
            (t.tm_hour << 11)
            | (t.tm_min << 5)
            | (t.tm_sec // 2)
        )

        fecha = (
            ((t.tm_year - 1980) << 9)
            | (t.tm_mon << 5)
            | t.tm_mday
        )

        return fecha, hora

    def _hueco_en_dir(self, clusters):

        for cl in clusters:

            base = self._cluster_offset(cl)

            self.f.seek(base)

            datos = self.f.read(self.bytes_cluster)

            for i in range(0, len(datos), 32):

                if datos[i] in (0x00, 0xE5):

                    return base + i

        return None

    def _extender_dir(self, clusters):

        nuevo = self._asignar_cadena(1)

        self.f.seek(self._cluster_offset(nuevo[0]))

        self.f.write(bytes(self.bytes_cluster))

        self._fat_escribir(clusters[-1], nuevo[0])

        return nuevo[0]

    def escribir(self, ruta, datos):

        partes = [
            p for p in ruta.replace("\\", "/").split("/")
            if p
        ]

        if not partes:

            raise FileNotFoundError("ruta vacia")

        dir_clusters, previa = self._buscar(ruta)

        # 1. Datos a clusters nuevos.

        cantidad = (
            len(datos) + self.bytes_cluster - 1
        ) // self.bytes_cluster

        cadena = self._asignar_cadena(cantidad)

        for i, cl in enumerate(cadena):

            trozo = datos[
                i * self.bytes_cluster:
                (i + 1) * self.bytes_cluster
            ]

            self.f.seek(self._cluster_offset(cl))

            self.f.write(
                trozo.ljust(self.bytes_cluster, b"\x00")
            )

        primero = cadena[0] if cadena else 0

        fecha, hora = self._fecha_hora_fat()

        # 2. Entrada del directorio.

        if previa is not None:

            pos = previa["pos"]

            viejo = previa["cluster"]

        else:

            pos = self._hueco_en_dir(dir_clusters)

            if pos is None:

                cl = self._extender_dir(dir_clusters)

                pos = self._cluster_offset(cl)

            viejo = None

        entrada = bytearray(32)

        entrada[0:11] = self._nombre83(partes[-1])

        entrada[11] = 0x20  # archivo

        struct.pack_into("<H", entrada, 14, hora)

        struct.pack_into("<H", entrada, 16, fecha)

        struct.pack_into("<H", entrada, 18, fecha)

        struct.pack_into(
            "<H", entrada, 20, (primero >> 16) & 0xFFFF
        )

        struct.pack_into("<H", entrada, 22, hora)

        struct.pack_into("<H", entrada, 24, fecha)

        struct.pack_into(
            "<H", entrada, 26, primero & 0xFFFF
        )

        struct.pack_into("<I", entrada, 28, len(datos))

        self.f.seek(pos)

        self.f.write(bytes(entrada))

        # 3. La cadena vieja se libera al final.

        if viejo:

            self._liberar_cadena(viejo)

        self._actualizar_fsinfo()

        self.f.flush()

        os.fsync(self.f.fileno())

    def cerrar(self):

        self.f.close()

    def __enter__(self):

        return self

    def __exit__(self, *exc):

        self.cerrar()

        return False


def kolibri_fecha_texto(fecha, hora):

    dia = fecha & 0x1F

    mes = (fecha >> 5) & 0x0F

    anno = 1980 + ((fecha >> 9) & 0x7F)

    hh = (hora >> 11) & 0x1F

    mm = (hora >> 5) & 0x3F

    return f"{anno:04d}-{mes:02d}-{dia:02d} {hh:02d}:{mm:02d}"


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
        # MOVER BINARIO
        # ----------------------------------------------------

        self._make_button(
            top,
            "📦  MOVER BINARIO",
            self.move_binary
        ).pack(
            side=tk.LEFT,
            padx=4,
            pady=14
        )

        # ----------------------------------------------------
        # EJECUTAR
        # ----------------------------------------------------

        self._make_button(
            top,
            "🚀  EJECUTAR",
            self.execute_kolibrios
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
                "./bin/" + KPP_BIN.name,
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
                f"[KOLIBRIOS] {self._kolibri_name(bin_file.stem)} (así se verá dentro de KolibriOS)",
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
    # KOLIBRIOS - NOMBRE 8.3
    #
    # La misma regla que asm_env.sh (Mini Entorno ASM):
    #
    #   - pasar a mayúsculas
    #   - todo carácter fuera de [A-Z0-9_$-] se vuelve "_"
    #   - si pasa de 8 caracteres: los 6 primeros más un
    #     sufijo de 2 hex con el cksum del nombre original
    #     (suma % 256), con el cksum POSIX calculado en
    #     Python (kolibri_cksum): el mismo valor que daba
    #     el comando del sistema, sin depender de el
    # ========================================================

    def _kolibri_name(
        self,
        nombre
    ):

        validos = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$-"

        name = "".join(
            c if c in validos else "_"
            for c in nombre.upper()
        )

        if len(name) > 8:

            suma = kolibri_cksum(
                nombre.encode()
            )

            name = name[:6] + f"{suma % 256:02X}"

        return name

    # ========================================================
    # KOLIBRIOS - GUARDA DE UN SOLO ESCRITOR
    #
    # La imagen es un disco FAT: no admite dos escritores.
    # Con QEMU abierto, KolibriOS guarda la FAT en caché con
    # la foto vieja; si mtools escribe desde fuera en ese
    # momento, las dos versiones se pisan y el sistema de
    # archivos se rasga (incidente del 2026-11-11).
    #
    # Regla: con QEMU abierto no se mueve ningún binario y
    # no se abre un segundo QEMU. Primero se apaga KolibriOS
    # desde su propio menú; QEMU se cierra solo y sus
    # escrituras aterrizan completas.
    # ========================================================

    def _qemu_con_imagen(self):

        ruta = str(
            KOLIBRIOS_IMAGE
        )

        if os.name == "nt":

            return self._qemu_con_imagen_windows(
                ruta
            )

        try:

            cmdlines = list(
                Path("/proc").glob(
                    "[0-9]*/cmdline"
                )
            )

        except OSError:

            return False

        for cmdline in cmdlines:

            try:

                args = cmdline.read_bytes().split(
                    b"\x00"
                )

            except OSError:

                continue

            if not args or not args[0]:

                continue

            programa = Path(
                args[0].decode(
                    "latin-1",
                    errors="replace"
                )
            ).name

            if not programa.startswith(
                "qemu-system"
            ):

                continue

            for arg in args[1:]:

                if ruta in arg.decode(
                    "latin-1",
                    errors="replace"
                ):

                    return True

        return False

    def _qemu_con_imagen_windows(
        self,
        ruta
    ):

        # En Windows no hay /proc: se pregunta a CIM por la
        # linea de comandos de cada qemu-system vivo y se
        # busca la ruta de la imagen en ella.

        try:

            r = subprocess.run(
                [
                    "powershell",
                    "-NoProfile",
                    "-Command",
                    "Get-CimInstance Win32_Process "
                    "-Filter \"Name like 'qemu-system%'\" "
                    "| Select-Object -ExpandProperty CommandLine"
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                timeout=15
            )

        except Exception:

            return False

        salida = r.stdout.decode(
            "latin-1",
            errors="replace"
        ).lower()

        return ruta.lower() in salida


    # ========================================================
    # KOLIBRIOS - MOVER BINARIO
    #
    # La misma lógica que la opción KolibriOS de asm_env.sh,
    # paso a paso:
    #
    #   1. verificar la imagen FAT32 (offset 1048576)
    #   2. copiar el BIN con su nombre 8.3, sin extensión
    #   3. extraer AUTORUN.DAT
    #   4. registrar /SYS/<NOMBRE> "" 100 sin duplicar
    #   5. escribir AUTORUN.DAT en la imagen
    #   6. verificar: raíz de la imagen + cola de AUTORUN
    #
    # Los temporales se limpian siempre (finally), igual que
    # el Bash los borra en cada salida de error.
    # ========================================================

    def move_binary(self):

        if self._qemu_con_imagen():

            self.log(
                "[ERROR] QEMU está abierto con esta imagen.",
                "error"
            )

            self.log(
                "  Con KolibriOS en marcha no se toca la imagen desde fuera: la FAT no admite dos escritores.",
                "error"
            )

            self.log(
                "  Apaga KolibriOS desde su menú — QEMU se cierra solo — y vuelve a pulsar MOVER BINARIO.",
                "error"
            )

            return

        if self.process is not None:

            self.log(
                "[WARNING] Ya existe una compilación en ejecución.",
                "warning"
            )

            return

        stem = self.current_file.stem

        bin_file = OUTPUT_DIR / f"{stem}.bin"

        if not bin_file.exists():

            self.log(
                f"[ERROR] No existe el BIN: {bin_file}",
                "error"
            )

            self.log(
                "[ERROR] Primero compila el programa.",
                "error"
            )

            return

        nombre = self._kolibri_name(stem)

        autorun_tmp = OUTPUT_DIR / f"AUTORUN_{nombre}_tmp.DAT"

        self.log(
            "============================================================",
            "info"
        )

        self.log(
            "[KOLIBRIOS] Mover binario",
            "info"
        )

        self.log(
            f"[FUENTE]  {self.current_file.name}",
            "info"
        )

        self.log(
            f"[BIN]     {bin_file}",
            "info"
        )

        self.log(
            f"[NOMBRE]  {nombre} (sin extensión, como lo recibe KolibriOS)",
            "info"
        )

        self.log(
            f"[IMAGEN]  {KOLIBRIOS_IMAGE}",
            "info"
        )

        try:

            if not KOLIBRIOS_IMAGE.exists():

                self.log(
                    "[ERROR] No existe la imagen persistente:",
                    "error"
                )

                self.log(
                    f"  {KOLIBRIOS_IMAGE}",
                    "error"
                )

                return

            datos_bin = bin_file.read_bytes()

            try:

                fat = KolibriFAT32(
                    KOLIBRIOS_IMAGE,
                    KOLIBRIOS_OFFSET
                )

            except (OSError, ValueError) as error:

                self.log(
                    "[ERROR] No se pudo abrir la partición FAT32.",
                    "error"
                )

                self.log(
                    f"[DETALLE] {error}",
                    "error"
                )

                return

            with fat:

                # ------------------------------------------------
                # 1. VERIFICAR IMAGEN
                # ------------------------------------------------

                self.log(
                    "1. Verificando imagen...",
                    "info"
                )

                fat.listar_raiz()

                self.log(
                    "✔ Imagen FAT32 accesible.",
                    "ok"
                )

                # ------------------------------------------------
                # 2. COPIAR BIN
                # ------------------------------------------------

                self.log(
                    "2. Copiando BIN a KolibriOS...",
                    "info"
                )

                fat.escribir(
                    f"/{nombre}",
                    datos_bin
                )

                if fat.leer(f"/{nombre}") != datos_bin:

                    self.log(
                        "[ERROR] El BIN no se verificó al releerlo.",
                        "error"
                    )

                    return

                self.log(
                    "✔ BIN incorporado.",
                    "ok"
                )

                # ------------------------------------------------
                # 3. EXTRAER AUTORUN.DAT
                # ------------------------------------------------

                self.log(
                    "3. Preparando AUTORUN.DAT...",
                    "info"
                )

                autorun_tmp.unlink(
                    missing_ok=True
                )

                try:

                    datos_autorun = fat.leer(
                        "/KOLIBRIOS/SETTINGS/AUTORUN.DAT"
                    )

                except FileNotFoundError:

                    self.log(
                        "[ERROR] No se pudo extraer AUTORUN.DAT.",
                        "error"
                    )

                    return

                autorun_tmp.write_bytes(
                    datos_autorun
                )

                self.log(
                    "✔ AUTORUN.DAT extraído.",
                    "ok"
                )

                # ------------------------------------------------
                # 4. REGISTRAR EN AUTORUN
                # ------------------------------------------------

                self.log(
                    "4. Configurando AUTORUN...",
                    "info"
                )

                texto = autorun_tmp.read_bytes().decode(
                    "latin-1"
                )

                texto = texto.replace(
                    "\r\n",
                    "\n"
                ).replace(
                    "\r",
                    "\n"
                )

                linea = f'/SYS/{nombre} "" 100'

                lineas = texto.split("\n")

                if linea in lineas:

                    self.log(
                        "✔ El programa ya estaba registrado en AUTORUN.",
                        "ok"
                    )

                    self.log(
                        "  No se agregó una entrada duplicada.",
                        "ok"
                    )

                else:

                    if "### Mini Entorno ASM ###" not in lineas:

                        texto += "\n### Mini Entorno ASM ###\n"

                    texto += linea + "\n"

                    self.log(
                        "✔ AUTORUN configurado.",
                        "ok"
                    )

                autorun_tmp.write_bytes(
                    texto.encode("latin-1")
                )

                # ------------------------------------------------
                # 5. ESCRIBIR AUTORUN.DAT
                # ------------------------------------------------

                self.log(
                    "5. Escribiendo AUTORUN.DAT en la imagen...",
                    "info"
                )

                fat.escribir(
                    "/KOLIBRIOS/SETTINGS/AUTORUN.DAT",
                    autorun_tmp.read_bytes()
                )

                self.log(
                    "✔ AUTORUN.DAT actualizado.",
                    "ok"
                )

                autorun_tmp.unlink(
                    missing_ok=True
                )

                # ------------------------------------------------
                # 6. VERIFICAR
                # ------------------------------------------------

                self.log(
                    "6. Verificando imagen...",
                    "info"
                )

                self.log(
                    "Contenido de la raíz:",
                    "info"
                )

                for entrada in fat.listar_raiz():

                    if entrada["es_dir"]:

                        detalle = "<DIR>"

                    else:

                        detalle = str(
                            entrada["size"]
                        )

                    self.log(
                        f"{entrada['nombre']:<12} {detalle:>10}  "
                        + kolibri_fecha_texto(
                            entrada["fecha"],
                            entrada["hora"]
                        ),
                        "info"
                    )

                self.log(
                    "Últimas líneas de AUTORUN.DAT:",
                    "info"
                )

                ultimas = fat.leer(
                    "/KOLIBRIOS/SETTINGS/AUTORUN.DAT"
                ).decode(
                    "latin-1"
                ).splitlines()[-8:]

                for linea_autorun in ultimas:

                    self.log(
                        linea_autorun,
                        "info"
                    )

            self.status_label.configure(
                text="KOLIBRIOS OK",
                fg=GREEN
            )

            self.log(
                "============================================================",
                "ok"
            )

            self.log(
                "KOLIBRIOS LISTO.",
                "ok"
            )

            self.log(
                f"[PROGRAMA] {nombre}",
                "ok"
            )

            self.log(
                "El programa será ejecutado automáticamente por KolibriOS.",
                "ok"
            )

            self.log(
                "============================================================",
                "ok"
            )

        except Exception as error:

            self.log(
                f"[ERROR] Mover binario: {error}",
                "error"
            )

        finally:

            autorun_tmp.unlink(
                missing_ok=True
            )


    # ========================================================
    # KOLIBRIOS - EJECUTAR (QEMU)
    #
    # Arranca la imagen persistente en QEMU con los mismos
    # parámetros que asm_env.sh. El programa registrado en
    # AUTORUN.DAT se ejecuta solo al arrancar KolibriOS.
    # QEMU queda en su propia sesión: cerrar el Studio no
    # lo cierra.
    # ========================================================

    def execute_kolibrios(self):

        if self._qemu_con_imagen():

            self.log(
                "[ERROR] Ya hay un QEMU abierto con esta imagen.",
                "error"
            )

            self.log(
                "  Dos QEMU sobre la misma imagen la corrompen: cierra primero ese KolibriOS (apágalo desde su menú).",
                "error"
            )

            return

        if shutil.which("qemu-system-i386") is None:

            self.log(
                "[ERROR] Falta el comando: qemu-system-i386",
                "error"
            )

            if os.name == "nt":

                self.log(
                    "[INFO] Instala QEMU para Windows desde qemu.org y revisa que qemu-system-i386.exe esté en el PATH.",
                    "info"
                )

            else:

                self.log(
                    "[INFO] Instálalo con: sudo apt install qemu-system-x86",
                    "info"
                )

            return

        if not KOLIBRIOS_IMAGE.exists():

            self.log(
                "[ERROR] No existe la imagen persistente:",
                "error"
            )

            self.log(
                f"  {KOLIBRIOS_IMAGE}",
                "error"
            )

            return

        # ------------------------------------------------
        # GUARDA: EL BINARIO DEBE ESTAR DENTRO
        #
        # EJECUTAR solo arranca KolibriOS; no copia nada.
        # Sin esta comprobación, el botón abre un KolibriOS
        # donde el programa actual no existe, y el usuario
        # lo busca en vano (pasó el 2026-11-11 con
        # L03_016C). El orden del ciclo es COMPILAR ->
        # MOVER BINARIO -> EJECUTAR.
        # ------------------------------------------------

        if self.current_file is not None:

            nombre = self._kolibri_name(
                self.current_file.stem
            )

            try:

                with KolibriFAT32(
                    KOLIBRIOS_IMAGE,
                    KOLIBRIOS_OFFSET
                ) as fat:

                    dentro = fat.existe(
                        f"/{nombre}"
                    )

            except (OSError, ValueError):

                dentro = False

            if not dentro:

                self.log(
                    f"[ERROR] {nombre} todavía no está dentro de la imagen.",
                    "error"
                )

                self.log(
                    "  EJECUTAR solo arranca KolibriOS: no copia nada.",
                    "error"
                )

                self.log(
                    "  El orden es COMPILAR -> MOVER BINARIO -> EJECUTAR: pulsa MOVER BINARIO y después EJECUTAR.",
                    "error"
                )

                return

        if os.name == "nt":

            # Windows: sin KVM. Emulacion TCG con el CPU
            # por defecto; para KolibriOS basta y sobra.
            # Sin -display: el QEMU de Windows abre su
            # ventana nativa.

            comando = [
                "qemu-system-i386",
                "-m", KOLIBRIOS_QEMU_MEM,
                "-drive", f"file={KOLIBRIOS_IMAGE},format=raw,if=ide",
                "-boot", "c"
            ]

        else:

            comando = [
                "qemu-system-i386",
                "-enable-kvm",
                "-cpu", "host",
                "-m", KOLIBRIOS_QEMU_MEM,
                "-drive", f"file={KOLIBRIOS_IMAGE},format=raw,if=ide",
                "-boot", "c",
                "-display", "gtk"
            ]

        try:

            if os.name == "nt":

                proceso = subprocess.Popen(
                    comando,
                    creationflags=subprocess.CREATE_NEW_PROCESS_GROUP
                )

            else:

                proceso = subprocess.Popen(
                    comando,
                    start_new_session=True
                )

        except OSError as error:

            self.log(
                f"[ERROR] No se pudo lanzar QEMU: {error}",
                "error"
            )

            return

        self.status_label.configure(
            text="QEMU KOLIBRIOS",
            fg=CYAN
        )

        self.log(
            "[EJECUTAR] KolibriOS arrancando en QEMU...",
            "ok"
        )

        self.log(
            f"[QEMU] PID {proceso.pid} — imagen: {KOLIBRIOS_IMAGE}",
            "info"
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
