#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Re-export an identity for Apple's PKCS#12 importer, entirely locally.

python3 tools/reexport-p12.py [input.p12] [-o compatible.p12]
The password is prompted on the terminal and retained for the new encrypted P12.
Decrypted key material uses memory/pipes only; existing files are never replaced.
OPENSSL may name an alternative OpenSSL executable.
"""
import argparse
import base64
import getpass
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import warnings


class ReexportError(Exception):
    pass


def openssl_path():
    candidates = [os.environ.get("OPENSSL"), "/opt/homebrew/bin/openssl",
                  "/usr/local/opt/openssl@3/bin/openssl", shutil.which("openssl")]
    for candidate in candidates:
        if candidate and pathlib.Path(candidate).is_file() and os.access(candidate, os.X_OK):
            return candidate
    raise ReexportError("OpenSSL не найден. Установи его через Homebrew или укажи путь в OPENSSL.")


def run_openssl(executable, arguments, *, data=None, password=None, pass_option=None):
    """Passwords travel over a separate inherited pipe, not argv or environment."""
    read_fd = write_fd = None
    try:
        command = [executable, *arguments]
        descriptors = ()
        if password is not None:
            encoded = password.encode("utf-8")
            if any(c in encoded for c in (b"\n", b"\r", b"\0")) or len(encoded) > 4094:
                raise ReexportError("Этот скрипт не поддерживает многострочный пароль, NUL или пароль длиннее 4094 байт.")
            read_fd, write_fd = os.pipe()
            os.write(write_fd, encoded + b"\n")
            os.close(write_fd)
            write_fd = None
            command += [pass_option, f"fd:{read_fd}"]
            descriptors = (read_fd,)
        result = subprocess.run(command, input=data if data is not None else b"", stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, pass_fds=descriptors)
        if result.returncode:
            # OpenSSL's raw output and all in-memory PEM data stay private.
            raise ReexportError("OpenSSL не смог обработать идентичность. Проверь пароль и наличие сертификата с приватным ключом.")
        return result.stdout
    finally:
        for descriptor in (read_fd, write_fd):
            if descriptor is not None:
                os.close(descriptor)


def certificates(pem):
    blocks = re.findall(rb"-----BEGIN CERTIFICATE-----\s*(.*?)\s*-----END CERTIFICATE-----", pem, re.DOTALL)
    return {base64.b64decode(re.sub(rb"\s+", b"", block), validate=True) for block in blocks}


def reexport(source, destination, password, executable=None):
    source, destination = pathlib.Path(source), pathlib.Path(destination)
    if destination.exists() or destination.is_symlink():
        raise ReexportError("Выходной файл уже существует; выбери другое имя. Он не был изменён.")
    if source.resolve() == destination.resolve():
        raise ReexportError("Исходный файл нельзя перезаписывать.")
    executable = executable or openssl_path()
    identity = run_openssl(executable, ["pkcs12", "-in", str(source), "-nodes"],
                           password=password, pass_option="-passin")
    original_certs = certificates(identity)
    if not original_certs:
        raise ReexportError("В исходном .p12 нет сертификата.")
    public_key = run_openssl(executable, ["pkey", "-pubout", "-outform", "DER"], data=identity)
    # OpenSSL reopens its certificate input while exporting. Give it a temporary
    # PUBLIC-certificate file; the private key is read once from stdin instead.
    with tempfile.TemporaryDirectory(prefix="husk-p12-public-certs-") as work:
        cert_file = pathlib.Path(work) / "certificates.pem"
        blocks = re.findall(rb"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", identity, re.DOTALL)
        cert_file.write_bytes(b"\n".join(blocks) + b"\n")
        converted = run_openssl(executable, ["pkcs12", "-export", "-in", str(cert_file), "-inkey", "/dev/stdin",
                                             "-name", "Husk signing identity", "-keypbe", "PBE-SHA1-3DES",
                                             "-certpbe", "PBE-SHA1-3DES", "-macalg", "sha1", "-iter", "2048"],
                                 data=identity, password=password, pass_option="-passout")
    # Independently reopen the result, verify its MAC, and check the full certificate
    # set and key identity before writing any output file.
    reopened = run_openssl(executable, ["pkcs12", "-in", "/dev/stdin", "-nodes"],
                           data=converted, password=password, pass_option="-passin")
    converted_key = run_openssl(executable, ["pkey", "-pubout", "-outform", "DER"], data=reopened)
    if certificates(reopened) != original_certs or converted_key != public_key:
        raise ReexportError("Проверка сохранности сертификатов или ключа не прошла. Выходной файл не создан.")
    descriptor = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(descriptor, "wb") as output:
            output.write(converted)
    except BaseException:
        destination.unlink(missing_ok=True)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", nargs="?", help="Исходный .p12; без аргумента путь будет запрошен")
    parser.add_argument("-o", "--output", help="Новый файл (по умолчанию рядом с исходным, с суффиксом -husk-compatible)")
    args = parser.parse_args()
    raw = args.input
    if raw is None:
        raw = input("Перетащи исходный .p12 сюда и нажми Enter: ").strip()
        # macOS Terminal may paste a shell-escaped or quoted path on file drop.
        if not pathlib.Path(raw).expanduser().is_file():
            parts = shlex.split(raw)
            if len(parts) == 1:
                raw = parts[0]
    source = pathlib.Path(raw).expanduser()
    if not source.is_file():
        raise ReexportError("Исходный .p12 не найден.")
    destination = pathlib.Path(args.output).expanduser() if args.output else source.with_name(source.stem + "-husk-compatible.p12")
    if destination.exists() or destination.is_symlink():
        raise ReexportError("Выходной файл уже существует; укажи другое имя через -o.")
    executable = openssl_path()
    with warnings.catch_warnings():
        warnings.simplefilter("error", getpass.GetPassWarning)
        try:
            password = getpass.getpass("Пароль исходного .p12 (ввод скрыт): ")
        except getpass.GetPassWarning:
            raise ReexportError("Запусти скрипт в Terminal: здесь невозможно скрыть ввод пароля.") from None
    reexport(source, destination, password, executable)
    print(f"Готово: {destination}")
    print("Сертификаты и ключ проверены. Пароль остался прежним. Импортируй новый файл в Husk 149.1.3.")


if __name__ == "__main__":
    try:
        main()
    except (ReexportError, OSError, ValueError, EOFError) as error:
        print(f"Ошибка: {error}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nОтменено.", file=sys.stderr)
        sys.exit(130)
