#!/usr/bin/env python3
# ============================================================
# CONSOLA DE REBANADAS - LOGICA PURA (S-1 Y S-2)
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
# Contenido de S-2:
#
#   extrae_volcado(...)  - saca un volcado de la imagen;
#                          si no esta, devuelve el listado
#                          de la raiz (leccion M-1).
#   leer_valores(datos)  - bytes a lista de enteros.
#   coteja_cola(...)     - el volcado contra la cola del
#                          esperado: tamano, cola exacta y
#                          alineamiento unico, con
#                          cobertura por movimientos.
#   coteja_segmentos(...)- los segmentos, en orden,
#                          contra la corriente completa.
#
# Vive en la raiz del clon, junto a kpp_bootstrap_studio.py.
# ============================================================

import hashlib
import os
import subprocess
from array import array
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


# ============================================================
# COTEJO - EXTRAER EL VOLCADO DE LA IMAGEN
#
# Saca los bytes de un volcado de la raiz de la imagen.
# Si el nombre no esta, devuelve (None, lineas) con el
# listado de la raiz: el nombre del volcado lo fija la
# Konsole al guardar, no el binario (leccion M-1), y ante
# la duda el listado es la verdad.
#
# La guarda de un solo escritor NO va aqui: la pone el
# manejador antes de llamar, igual que en SWITCH. Con la
# VM abierta no se toca la imagen ni para leer.
# ============================================================

def extrae_volcado(
    fat_factory,
    ruta_imagen,
    offset,
    nombre
):

    lineas = []

    nombre = str(nombre).strip().lstrip("/")

    if not nombre:

        lineas.append(
            "[ERROR] COTEJO: nombre del volcado vacio."
        )

        return None, lineas

    if not Path(ruta_imagen).is_file():

        lineas.append(
            f"[ERROR] COTEJO: no existe la imagen: {ruta_imagen}"
        )

        return None, lineas

    try:

        fat = fat_factory(
            str(ruta_imagen),
            offset
        )

    except Exception as error:

        lineas.append(
            f"[ERROR] COTEJO: no se pudo abrir la imagen: {error}"
        )

        return None, lineas

    with fat:

        if not fat.existe("/" + nombre):

            lineas.append(
                f"{nombre} no esta en la raiz de la imagen."
            )

            lineas.append(
                "Contenido de la raiz:"
            )

            for entrada in fat.listar_raiz():

                if entrada["es_dir"]:

                    marca = "(dir)"

                else:

                    marca = str(entrada["size"])

                lineas.append(
                    f"    {entrada['nombre']}  {marca}"
                )

            return None, lineas

        datos = fat.leer("/" + nombre)

    lineas.append(
        f"Extraido de la imagen: {nombre} ({len(datos)} bytes)"
    )

    return datos, lineas


# ============================================================
# COTEJO - LECTURA DE VALORES Y ALINEAMIENTOS
#
# leer_valores trocea los bytes por espacios: los
# archivos de valores son inmunes a CRLF/LF.
#
# _buscar_alineamientos da las posiciones 0-basadas
# donde la secuencia real aparece entera en la
# corriente esperada. Las rachas de ceros del DATA
# podrian hacer calzar un volcado en un sitio falso:
# la unicidad se demuestra, no se asume.
# ============================================================

TOPE_SEGMENTO = 512


def leer_valores(datos):

    texto = datos.decode(
        "ascii",
        errors="replace"
    )

    return [int(v) for v in texto.split()]


def _buscar_alineamientos(real, esperado):

    a_real = array(
        "l",
        real
    )

    a_esp = array(
        "l",
        esperado
    )

    aguja = a_real.tobytes()

    pajar = a_esp.tobytes()

    paso = a_real.itemsize

    hallados = []

    pos = pajar.find(aguja)

    while pos != -1:

        if pos % paso == 0:

            hallados.append(pos // paso)

        pos = pajar.find(aguja, pos + 1)

    return hallados


# ============================================================
# COTEJO - MODO COLA
#
# El runtime conserva un historial circular de 512
# registros: Copiar solo recupera los ultimos valores
# que la sonda imprimio. El cotejo es cola contra
# cola, con los tres candados de verifica_volcado.py:
#
#   1. tamano en bytes: valores unidos por CRLF, sin
#      salto final (se admite el salto final como
#      variante valida, igual que en el CLI)
#   2. cola exacta contra los ultimos N del esperado
#   3. alineamiento unico en toda la corriente
#
# rangos es una lista opcional de (nombre, inicio,
# fin) 1-basados: se informa la cobertura por
# movimiento. Devuelve (ok, lineas_resumen); ok es el
# veredicto de la cola, como en el CLI.
# ============================================================

def coteja_cola(real_bytes, esperado, rangos=None):

    lineas = []

    real = leer_valores(real_bytes)

    n = len(real)

    total = len(esperado)

    lineas.append(
        f"VALORES REALES: {n}"
    )

    lineas.append(
        f"VALORES ESPERADOS: {total}"
    )

    lineas.append(
        f"BYTES DEL VOLCADO: {len(real_bytes)}"
    )

    unidos = "\r\n".join(
        str(v) for v in real
    ).encode("ascii")

    if len(real_bytes) == len(unidos):

        lineas.append(
            "TAMANO: cuadra al byte (CRLF sin salto final)"
        )

    elif len(real_bytes) == len(unidos) + 2:

        lineas.append(
            "TAMANO: cuadra con salto final CRLF"
        )

    else:

        lineas.append(
            f"TAMANO: NO cuadra (esperado {len(unidos)} o "
            f"{len(unidos) + 2}, real {len(real_bytes)})"
        )

    if n == 0:

        lineas.append(
            "RESULTADO: el volcado no trae valores."
        )

        return False, lineas

    if n > total:

        lineas.append(
            "RESULTADO: el volcado tiene MAS valores que el esperado."
        )

        return False, lineas

    inicio_cola = total - n

    desajustes = [
        i for i in range(n)
        if real[i] != esperado[inicio_cola + i]
    ]

    if desajustes:

        i = desajustes[0]

        lineas.append(
            "RESULTADO: LA COLA NO ES EXACTA."
        )

        lineas.append(
            f"Primer desajuste: posicion global {inicio_cola + i + 1} "
            f"(valor {i + 1} del volcado): real={real[i]} "
            f"esperado={esperado[inicio_cola + i]}"
        )

        lineas.append(
            f"Desajustes totales contra la cola: {len(desajustes)}"
        )

        return False, lineas

    lineas.append(
        f"RESULTADO: COLA EXACTA {n}/{n} "
        f"(valores {inicio_cola + 1}-{total} del esperado)"
    )

    hallados = _buscar_alineamientos(real, esperado)

    if len(hallados) == 1 and hallados[0] == inicio_cola:

        lineas.append(
            f"ALINEAMIENTO: unico en toda la corriente "
            f"(inicio {inicio_cola + 1})"
        )

    elif inicio_cola in hallados:

        inicios = ", ".join(
            str(h + 1) for h in hallados
        )

        lineas.append(
            f"ALINEAMIENTO: OJO - la secuencia aparece "
            f"{len(hallados)} veces; la cola es una de ellas "
            f"(inicios: {inicios})"
        )

    else:

        lineas.append(
            "ALINEAMIENTO: la cola calza pero la secuencia "
            "no se reencontro; revisar."
        )

    if rangos:

        lineas.append("-" * 60)

        lineas.append(
            f"COBERTURA (volcado = valores {inicio_cola + 1}-{total}):"
        )

        for nombre_r, ini, fin in rangos:

            if fin < inicio_cola + 1 or ini > total:

                estado = "fuera (por el puerto)"

            elif ini >= inicio_cola + 1:

                estado = "ENTERO en campo"

            else:

                estado = f"parcial: desde el valor {inicio_cola + 1}"

            lineas.append(
                "   %-24s %6d-%-6d %s" % (nombre_r, ini, fin, estado)
            )

    return True, lineas


# ============================================================
# COTEJO - MODO SEGMENTOS
#
# La sonda segmentada (fallo de 2026-11-22): cada
# segmento imprime como maximo 512 valores y se copia
# antes de correr el siguiente. Los volcados, en
# orden, deben reconstruir la corriente COMPLETA del
# esperado: sin cola perdida y sin salvedad. Es la
# logica de verifica_segmentos.py; el desajuste dice
# tambien en que segmento cae.
# ============================================================

def coteja_segmentos(lista_bytes, esperado):

    lineas = []

    lineas.append(
        f"VALORES ESPERADOS: {len(esperado)}"
    )

    lineas.append(
        f"SEGMENTOS: {len(lista_bytes)}"
    )

    lineas.append("-" * 60)

    reconstruidos = []

    inicios = []

    desplazamiento = 0

    for i, datos in enumerate(lista_bytes, 1):

        valores = leer_valores(datos)

        ini = desplazamiento + 1

        fin = desplazamiento + len(valores)

        aviso = ""

        if len(valores) > TOPE_SEGMENTO:

            aviso = (
                f"  (OJO: >{TOPE_SEGMENTO}, "
                "no cabe en el historial)"
            )

        lineas.append(
            f"SEG {i:<2d} {len(valores):4d} valores  "
            f"(global {ini}-{fin}){aviso}"
        )

        inicios.append(ini)

        reconstruidos.extend(valores)

        desplazamiento = fin

    lineas.append("-" * 60)

    lineas.append(
        f"VALORES RECONSTRUIDOS: {len(reconstruidos)}"
    )

    if len(reconstruidos) != len(esperado):

        lineas.append(
            f"RESULTADO: LONGITUD DIFERENTE (reconstruidos "
            f"{len(reconstruidos)}, esperados {len(esperado)}, "
            f"diferencia {len(esperado) - len(reconstruidos)})"
        )

        return False, lineas

    for i, (r, e) in enumerate(zip(reconstruidos, esperado)):

        if r != e:

            seg = 1

            for j, ini in enumerate(inicios, 1):

                if ini <= i + 1:

                    seg = j

            lineas.append(
                f"RESULTADO: DESAJUSTE en la posicion global "
                f"{i + 1} (segmento {seg}): real={r} esperado={e}"
            )

            return False, lineas

    lineas.append(
        f"RESULTADO: CORRIENTE COMPLETA EXACTA "
        f"{len(reconstruidos)}/{len(esperado)} - sin salvedad"
    )

    return True, lineas
