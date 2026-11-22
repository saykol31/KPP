#!/usr/bin/env python3
# ============================================================
# CONSOLA DE REBANADAS - LOGICA PURA (S-1: SWITCH Y GUARDA VM)
#
# Funciones sin interfaz grafica para el circuito de
# rebanadas de Stage 1. La pestana REBANADAS del Studio
# (kpp_bootstrap_studio.py) las llama; tambien se pueden
# usar desde cualquier script de banco.
#
# Regla de la casa: este modulo NUNCA importa
# kpp_bootstrap_studio. Recibe lo que necesita como
# parametros (la clase KolibriFAT32 como fabrica, la ruta
# de la imagen, el offset de la particion y una funcion
# log), porque el Studio corre como __main__ e importarlo
# crearia una segunda instancia del modulo.
#
# Contenido de S-1:
#
#   vm_con_imagen(ruta)  - guarda de un solo escritor,
#                          ampliada: detecta QEMU y
#                          VirtualBox usando la imagen.
#   hacer_switch(...)    - mete una fuente en la imagen:
#                          la vieja sale a respaldo con
#                          sello de tiempo, la nueva entra,
#                          y todo se verifica con el triple
#                          hash MD5 (antes / local / despues)
#                          y byte a byte. Sin normalizar:
#                          entra exactamente lo leido de
#                          disco, con sus CRLF si los trae.
#
# Vive en la raiz del clon, junto a kpp_bootstrap_studio.py.
# ============================================================

import hashlib
import os
import subprocess
from datetime import datetime
from pathlib import Path


def _md5(datos):

    return hashlib.md5(datos).hexdigest()


# ============================================================
# GUARDA DE UN SOLO ESCRITOR (QEMU + VIRTUALBOX)
#
# Replica ampliada de la guarda del Studio
# (_qemu_con_imagen): la imagen FAT no admite dos
# escritores. La guarda vieja solo detecta procesos
# qemu-system*, pero en la maquina de campo KolibriOS
# corre en VirtualBox, cuyo proceso (VirtualBoxVM /
# VBoxHeadless) no menciona la ruta de la imagen en su
# linea de comandos: identifica la VM por su nombre, que
# es el nombre de la carpeta que contiene la imagen
# (~/VirtualBox VMs/<VM>/<imagen>). Por eso, para los
# procesos de VirtualBox se busca tambien ese nombre.
#
# Devuelve una descripcion del proceso hallado
# ("VirtualBoxVM (pid 1234)") o None si la imagen esta
# libre.
# ============================================================

def _tipo_vm(nombre_programa):

    n = nombre_programa.lower()

    if n.startswith("qemu-system"):

        return "qemu"

    if n.startswith("virtualboxvm") or n.startswith("vboxheadless"):

        return "vbox"

    return None


def _agujas(ruta, tipo):

    # Formas de buscar la imagen en una linea de comandos:
    # la ruta tal cual y con los separadores cambiados
    # (Windows mezcla \ y / segun quien lance el proceso).
    agujas = set()

    for variante in (
        ruta,
        ruta.replace("/", "\\"),
        ruta.replace("\\", "/")
    ):

        if variante:

            agujas.add(variante.lower())

    if tipo == "vbox":

        carpeta_vm = Path(ruta).parent.name

        if carpeta_vm:

            agujas.add(carpeta_vm.lower())

    return agujas


def _coincide(args, ruta):

    if not args or not args[0]:

        return None

    programa = Path(args[0]).name

    tipo = _tipo_vm(programa)

    if tipo is None:

        return None

    texto = " ".join(args).lower()

    for aguja in _agujas(ruta, tipo):

        if aguja in texto:

            return programa

    return None


def _vm_con_imagen_proc(ruta):

    try:

        cmdlines = list(
            Path("/proc").glob("[0-9]*/cmdline")
        )

    except OSError:

        return None

    for cmdline in cmdlines:

        try:

            args = [
                a.decode("latin-1", errors="replace")
                for a in cmdline.read_bytes().split(b"\x00")
                if a
            ]

        except OSError:

            continue

        programa = _coincide(args, ruta)

        if programa is not None:

            pid = cmdline.parent.name

            return f"{programa} (pid {pid})"

    return None


def _vm_con_imagen_windows(ruta):

    # En Windows no hay /proc: se pregunta a CIM por los
    # procesos de VM vivos y se revisa su linea de
    # comandos en Python (la guarda vieja del Studio solo
    # buscaba qemu-system y solo la ruta).
    comando = (
        "Get-CimInstance Win32_Process "
        "-Filter \"Name like 'qemu-system%' "
        "or Name like 'VirtualBoxVM%' "
        "or Name like 'VBoxHeadless%'\" "
        "| ForEach-Object { "
        "\"$($_.ProcessId)|$($_.Name)|$($_.CommandLine)\" }"
    )

    try:

        r = subprocess.run(
            [
                "powershell",
                "-NoProfile",
                "-Command",
                comando
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=15
        )

    except Exception:

        return None

    salida = r.stdout.decode(
        "latin-1",
        errors="replace"
    )

    for linea in salida.splitlines():

        partes = linea.split("|", 2)

        if len(partes) != 3:

            continue

        pid, nombre, cmdline = partes

        programa = _coincide(
            [nombre] + cmdline.split(),
            ruta
        )

        if programa is not None:

            return f"{nombre} (pid {pid})"

    return None


def vm_con_imagen(ruta_imagen):

    ruta = str(ruta_imagen)

    if os.name == "nt":

        return _vm_con_imagen_windows(ruta)

    return _vm_con_imagen_proc(ruta)


# ============================================================
# SWITCH - METER UNA FUENTE EN LA IMAGEN
#
# El gesto de mete_fuente.py, generalizado:
#
#   1. leer la fuente local y su MD5
#   2. si el destino ya existe en la imagen, leer la
#      vieja y guardarla en
#      <raiz_respaldo>/switch/<NOMBRE>/<AAAAMMDD_HHMMSS>
#   3. escribir la nueva
#   4. releer y comparar byte a byte y por MD5
#
# fat_factory es la clase KolibriFAT32 (se llama como
# fat_factory(ruta_imagen, offset)). log recibe una
# linea de narracion por paso. Devuelve
# (ok, lineas_resumen): el resumen trae el triple hash
# (imagen antes / local / imagen despues) y los tamanos.
#
# Sin normalizacion de ninguna clase: se mete
# exactamente lo leido de disco; si la fuente viaja con
# CRLF, el hash lo delata en vez de ocultarlo.
# ============================================================

def hacer_switch(
    fat_factory,
    ruta_imagen,
    offset,
    ruta_local,
    nombre_destino,
    raiz_respaldo,
    log
):

    resumen = []

    def decir(linea):

        resumen.append(linea)

        if log is not None:

            log(linea)

    nombre = str(nombre_destino).strip().lstrip("/")

    if not nombre:

        decir("[ERROR] SWITCH: nombre destino vacio.")

        return False, resumen

    destino = "/" + nombre

    local = Path(ruta_local)

    if not local.is_file():

        decir(
            f"[ERROR] SWITCH: no existe la fuente local: {local}"
        )

        return False, resumen

    if not Path(ruta_imagen).is_file():

        decir(
            f"[ERROR] SWITCH: no existe la imagen: {ruta_imagen}"
        )

        return False, resumen

    datos = local.read_bytes()

    md5_local = _md5(datos)

    decir(
        f"SWITCH -> {destino}"
    )

    decir(
        f"  local:            {local.name}  "
        f"{len(datos)} bytes  md5 {md5_local}"
    )

    try:

        fat = fat_factory(
            str(ruta_imagen),
            offset
        )

    except Exception as error:

        decir(
            f"[ERROR] SWITCH: no se pudo abrir la imagen: {error}"
        )

        return False, resumen

    with fat:

        if fat.existe(destino):

            vieja = fat.leer(destino)

            md5_antes = _md5(vieja)

            decir(
                f"  imagen (antes):   "
                f"{len(vieja)} bytes  md5 {md5_antes}"
            )

            carpeta = (
                Path(raiz_respaldo) / "switch" / nombre
            )

            carpeta.mkdir(
                parents=True,
                exist_ok=True
            )

            sello = datetime.now().strftime(
                "%Y%m%d_%H%M%S"
            )

            respaldo = carpeta / sello

            extra = 2

            while respaldo.exists():

                respaldo = carpeta / f"{sello}_{extra}"

                extra += 1

            respaldo.write_bytes(vieja)

            decir(
                f"  respaldo:         {respaldo}"
            )

        else:

            decir(
                "  imagen (antes):   no existia"
            )

        fat.escribir(destino, datos)

        vuelta = fat.leer(destino)

    md5_despues = _md5(vuelta)

    decir(
        f"  imagen (despues): "
        f"{len(vuelta)} bytes  md5 {md5_despues}"
    )

    if vuelta == datos and md5_despues == md5_local:

        decir(
            "  verificacion:     byte a byte OK"
        )

        return True, resumen

    decir(
        "  verificacion:     FALLO - lo leido no coincide "
        "con lo escrito"
    )

    return False, resumen
