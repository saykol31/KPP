#!/usr/bin/env python3
# ============================================================
# CONSOLA DE REBANADAS - LOGICA PURA (S-1, S-2 Y S-3)
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
# Contenido de S-3:
#
#   lee_manifiesto(texto)- el manifiesto de la rebanada
#                          (pares CLAVE=valor) a dict.
#   compara_corrientes() - dos corrientes de valores:
#                          exacta, o el primer desajuste
#                          con posicion y ambos valores.
#   corre_puerto(...)    - corre un puerto por el
#                          contrato (conserva/genera) y
#                          devuelve su corriente.
#   verifica_canario_bin()- el BIN contra el tamano y
#                          el MD5 fijados en el
#                          manifiesto.
#   corre_suite(...)     - validar_test.sh --no-build
#                          con su resumen parseado; None
#                          si la suite no puede correr
#                          en la maquina (sin bash, o
#                          bin/kpp no ejecutable).
#
# Vive en la raiz del clon, junto a kpp_bootstrap_studio.py.
# ============================================================

import hashlib
import os
import re
import shutil
import subprocess
import sys
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


# ============================================================
# ORACULO - MANIFIESTO
#
# El manifiesto de una rebanada es un texto de pares
# CLAVE=valor, uno por linea. Las lineas vacias y los
# comentarios (#) se ignoran; las claves se guardan en
# mayusculas. Es el contrato entre la carpeta de la
# rebanada y el boton ORACULO: PUERTO,
# ESPERADO_ANTERIOR, ESPERADO, CANARIO_BYTES,
# CANARIO_MD5, STEM, BIN.
# ============================================================

def lee_manifiesto(texto):

    datos = {}

    for linea in texto.splitlines():

        linea = linea.strip()

        if not linea or linea.startswith("#"):

            continue

        if "=" not in linea:

            continue

        clave, valor = linea.split("=", 1)

        datos[clave.strip().upper()] = valor.strip()

    return datos


# ============================================================
# ORACULO - COMPARAR DOS CORRIENTES
#
# La comparacion que las fases del oraculo necesitan:
# la corriente del puerto contra un esperado ya
# escrito. Exacta con conteos, o el primer desajuste
# con su posicion 1-basada y ambos valores, o la
# diferencia de longitud (con el primer desajuste del
# tramo comun, si lo hay).
# ============================================================

def compara_corrientes(
    real,
    esperado,
    nombre_real="puerto",
    nombre_esperado="esperado"
):

    lineas = []

    lineas.append(
        f"{nombre_real}: {len(real)} valores"
    )

    lineas.append(
        f"{nombre_esperado}: {len(esperado)} valores"
    )

    if len(real) != len(esperado):

        lineas.append(
            f"RESULTADO: LONGITUD DIFERENTE ({nombre_real} "
            f"{len(real)}, {nombre_esperado} {len(esperado)}, "
            f"diferencia {len(esperado) - len(real)})"
        )

        comun = min(len(real), len(esperado))

        for i in range(comun):

            if real[i] != esperado[i]:

                lineas.append(
                    f"Primer desajuste dentro del tramo comun: "
                    f"posicion {i + 1}: {nombre_real}={real[i]} "
                    f"{nombre_esperado}={esperado[i]}"
                )

                break

        return False, lineas

    for i, (r, e) in enumerate(zip(real, esperado)):

        if r != e:

            lineas.append(
                f"RESULTADO: DESAJUSTE en la posicion {i + 1}: "
                f"{nombre_real}={r} {nombre_esperado}={e}"
            )

            return False, lineas

    lineas.append(
        f"RESULTADO: CORRIENTE EXACTA {len(real)}/{len(esperado)} "
        f"({nombre_real} == {nombre_esperado})"
    )

    return True, lineas


# ============================================================
# ORACULO - CORRER EL PUERTO (CONTRATO DE PUERTO)
#
# Contrato que rige para los puertos nuevos (los
# viejos no se reescriben):
#
#   python puerto.py conserva
#       imprime su corriente a stdout, exit 0.
#   python puerto.py genera <ruta>
#       escribe su corriente al archivo dado, exit 0.
#   cualquier otro uso: exit != 0.
#
# En conserva se captura stdout y se trocea a valores;
# en genera se exige el archivo de salida y se lee a
# valores. Devuelve (ok, lineas, valores|None). Los
# errores del puerto (codigo de salida, tiempo
# agotado, salida que no es una corriente de enteros)
# salen como lineas claras, con el stderr recortado a
# sus ultimas lineas.
# ============================================================

def corre_puerto(
    ruta_puerto,
    modo,
    ruta_salida=None,
    cwd=None,
    timeout=600
):

    lineas = []

    if modo not in ("conserva", "genera"):

        lineas.append(
            f"[ERROR] PUERTO: modo desconocido: {modo} "
            "(conserva|genera)"
        )

        return False, lineas, None

    ruta = Path(ruta_puerto)

    if not ruta.is_file():

        lineas.append(
            f"[ERROR] PUERTO: no existe el puerto: {ruta}"
        )

        return False, lineas, None

    comando = [
        sys.executable,
        str(ruta),
        modo
    ]

    if modo == "genera":

        if ruta_salida is None:

            lineas.append(
                "[ERROR] PUERTO: el modo genera exige "
                "ruta de salida."
            )

            return False, lineas, None

        comando.append(str(ruta_salida))

    try:

        r = subprocess.run(
            comando,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=str(cwd) if cwd is not None else None,
            timeout=timeout
        )

    except subprocess.TimeoutExpired:

        lineas.append(
            f"[ERROR] PUERTO: tiempo agotado ({timeout} s) "
            f"en modo {modo}."
        )

        return False, lineas, None

    except OSError as error:

        lineas.append(
            f"[ERROR] PUERTO: no se pudo ejecutar: {error}"
        )

        return False, lineas, None

    if r.returncode != 0:

        lineas.append(
            f"[ERROR] PUERTO: salio con codigo {r.returncode} "
            f"en modo {modo}."
        )

        stderr = r.stderr.decode(
            "utf-8",
            errors="replace"
        ).strip()

        if stderr:

            for linea_err in stderr.splitlines()[-6:]:

                lineas.append(
                    f"  puerto: {linea_err}"
                )

        return False, lineas, None

    if modo == "conserva":

        datos = r.stdout

        origen = "la salida del puerto"

    else:

        salida = Path(ruta_salida)

        if not salida.is_file():

            lineas.append(
                "[ERROR] PUERTO: el modo genera no creo "
                f"el archivo: {salida}"
            )

            return False, lineas, None

        datos = salida.read_bytes()

        origen = f"el archivo {salida.name}"

    try:

        valores = leer_valores(datos)

    except ValueError:

        lineas.append(
            f"[ERROR] PUERTO: {origen} no es una "
            "corriente de enteros."
        )

        return False, lineas, None

    lineas.append(
        f"PUERTO ({modo}): {len(valores)} valores"
    )

    return True, lineas, valores


# ============================================================
# ORACULO - CANARIO BIN
#
# El canario de una rebanada: el BIN que la pieza
# produce debe dar el tamano y el MD5 fijados en el
# manifiesto (CANARIO_BYTES y CANARIO_MD5). Sin esos
# dos datos el canario esta incompleto y acusa.
# ============================================================

def verifica_canario_bin(ruta_bin, bytes_esperados, md5_esperado):

    lineas = []

    ruta = Path(ruta_bin)

    lineas.append(
        f"BIN: {ruta}"
    )

    if bytes_esperados is None or md5_esperado is None:

        lineas.append(
            "[ERROR] CANARIO BIN: faltan CANARIO_BYTES o "
            "CANARIO_MD5 en el manifiesto."
        )

        return False, lineas

    try:

        bytes_esp = int(bytes_esperados)

    except (TypeError, ValueError):

        lineas.append(
            f"[ERROR] CANARIO BIN: CANARIO_BYTES no es "
            f"un entero: {bytes_esperados}"
        )

        return False, lineas

    md5_esp = str(md5_esperado).strip().lower()

    if not ruta.is_file():

        lineas.append(
            "[ERROR] CANARIO BIN: no existe el BIN."
        )

        return False, lineas

    datos = ruta.read_bytes()

    md5_real = _md5(datos)

    ok = True

    if len(datos) == bytes_esp:

        lineas.append(
            f"  tamano: {len(datos)} bytes "
            f"(esperado {bytes_esp}) OK"
        )

    else:

        lineas.append(
            f"  tamano: {len(datos)} bytes "
            f"(esperado {bytes_esp}) FALLO"
        )

        ok = False

    if md5_real == md5_esp:

        lineas.append(
            f"  md5:    {md5_real} (esperado {md5_esp}) OK"
        )

    else:

        lineas.append(
            f"  md5:    {md5_real} (esperado {md5_esp}) FALLO"
        )

        ok = False

    lineas.append(
        "CANARIO BIN: " + ("OK" if ok else "FALLO")
    )

    return ok, lineas


# ============================================================
# ORACULO - SUITE DE REGRESION
#
# Corre validar_test.sh --no-build en la raiz del
# repo y parsea su resumen real:
#
#   PASS positivos : N
#   PASS negativos : N
#   FAIL           : N
#
# Devuelve (ok, lineas); ok es True solo si FAIL es 0
# y el script salio con codigo 0. Si la suite no puede
# correr en la maquina devuelve (None, lineas): None
# es "no disponible", no un fallo, y la pata se
# declara sin bloquear a las demas. Dos casos: no hay
# bash ni sh, o el binario que la suite prueba
# (bin/kpp, el ELF del Stage 0) no es ejecutable aqui
# - el de un clon de Windows.
# ============================================================

def corre_suite(ruta_raiz, timeout=1800):

    lineas = []

    bash = shutil.which("bash") or shutil.which("sh")

    if bash is None:

        lineas.append(
            "SUITE: no disponible en esta maquina "
            "(sin bash ni sh)."
        )

        return None, lineas

    raiz = Path(ruta_raiz)

    script = raiz / "validar_test.sh"

    if not script.is_file():

        lineas.append(
            f"[ERROR] SUITE: no existe {script}"
        )

        return False, lineas

    # La suite prueba el binario del Stage 0 (bin/kpp):
    # donde ese binario no es ejecutable - un clon de
    # Windows, cuyo arbol trae el ELF de Linux - la
    # suite no puede correr y se declara no disponible,
    # como cuando no hay bash. Se le pregunta al propio
    # bash, que es quien la va a correr.
    try:

        sonda = subprocess.run(
            [
                bash,
                "-c",
                "test -x bin/kpp"
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            cwd=str(raiz),
            timeout=30
        )

        ejecutable = sonda.returncode == 0

    except (OSError, subprocess.TimeoutExpired):

        ejecutable = False

    if not ejecutable:

        lineas.append(
            "SUITE: no disponible en esta maquina "
            "(bin/kpp no es ejecutable aqui; la suite "
            "es del lado Linux)."
        )

        return None, lineas

    try:

        r = subprocess.run(
            [
                bash,
                "validar_test.sh",
                "--no-build"
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=str(raiz),
            timeout=timeout
        )

    except subprocess.TimeoutExpired:

        lineas.append(
            f"[ERROR] SUITE: tiempo agotado ({timeout} s)."
        )

        return False, lineas

    except OSError as error:

        lineas.append(
            f"[ERROR] SUITE: no se pudo ejecutar: {error}"
        )

        return False, lineas

    salida = r.stdout.decode(
        "utf-8",
        errors="replace"
    )

    m_pos = re.search(
        r"^PASS positivos\s*:\s*(\d+)",
        salida,
        re.MULTILINE
    )

    m_neg = re.search(
        r"^PASS negativos\s*:\s*(\d+)",
        salida,
        re.MULTILINE
    )

    m_fail = re.search(
        r"^FAIL\s*:\s*(\d+)",
        salida,
        re.MULTILINE
    )

    if m_pos is None or m_neg is None or m_fail is None:

        lineas.append(
            "[ERROR] SUITE: no se pudo parsear el resumen "
            "de validar_test.sh."
        )

        for linea_out in salida.splitlines()[-4:]:

            lineas.append(
                f"  suite: {linea_out}"
            )

        return False, lineas

    n_pos = int(m_pos.group(1))

    n_neg = int(m_neg.group(1))

    n_fail = int(m_fail.group(1))

    lineas.append(
        f"SUITE: PASS {n_pos} / PASS neg {n_neg} / FAIL {n_fail}"
    )

    ok = n_fail == 0 and r.returncode == 0

    if r.returncode != 0 and n_fail == 0:

        lineas.append(
            f"  (validar_test.sh salio con codigo "
            f"{r.returncode})"
        )

    lineas.append(
        "SUITE: " + ("OK" if ok else "FALLO")
    )

    return ok, lineas
