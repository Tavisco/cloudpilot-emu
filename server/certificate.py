import os
import random
import re
import readline
import sys

from OpenSSL import crypto

CN_DEFAULT = "cloudpilot-server"
REGEX_IP = re.compile('^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$')
REGEX_NAME = re.compile('^[a-zA-Z\d\.\-]+$')
VALIDITY_YEARS = 2

def _deleteIfRequired(file, overwrite):
    if not os.path.exists(file):
        return

    if os.path.isdir(file):
        print(f'{file} exists and is a directory, aborting...')
        exit(1)

    if not overwrite:
        print(f'{file} exists, aborting... specifiy "--overwrite" if you want to overwrite it instead')
        exit(1)

    try:
        os.remove(file)
    except Exception as ex:
        print(f'ERROR: unable to delete {file}: {ex}')
        exit(1)

def _inputNames():
    print('please enter a comma separated list of IPs, hostnames or domains for which this cert will be valid:')
    namestring = input()

    parts = [name.strip() for name in namestring.split(",")]
    ips = []
    names = []

    for part in parts:
        if REGEX_IP.match(part):
            ips.append(part)

        elif REGEX_NAME.match(part):
            names.append(part)

        else:
            print(f'ERROR: {part} is neither a valid IP, name or domain')
            return (None, None)

    return (ips, names)

def generateCertificate(overwrite):
    cn = input(f'certificate name (enter for {CN_DEFAULT}): ')
    cn = cn if cn else CN_DEFAULT
    print()

    filePem = cn + ".pem"
    fileCer = cn + ".cer"

    _deleteIfRequired(filePem, overwrite)
    _deleteIfRequired(fileCer, overwrite)

    ips = None
    names = None
    while ips == None or names == None:
        ips, names = _inputNames()
        print()

    print("generating key and certificate...")

    key = crypto.PKey()
    key.generate_key(crypto.TYPE_RSA, 4096)

    cert = crypto.X509()
    cert.set_version(2)
    cert.get_subject().CN = cn

    basicConstraints = crypto.X509Extension(b"basicConstraints", True, b"CA:TRUE,pathlen:0")
    subjectAltName = crypto.X509Extension(b"subjectAltName", False,
        bytes(",".join([f'IP:{ip}' for ip in ips] + [f'DNS:{name}' for name in names]), "utf8")
    )
    cert.add_extensions((basicConstraints, subjectAltName))

    random.seed()
    cert.set_serial_number(random.randint(0, 0xffff))

    cert.gmtime_adj_notBefore(0)
    cert.gmtime_adj_notAfter(VALIDITY_YEARS * 365 * 24 * 3600)

    cert.set_issuer(cert.get_subject())
    cert.set_pubkey(key)
    cert.sign(key, "sha256")

    try:
        with open(filePem, "wb") as file:
            file.write(crypto.dump_certificate(crypto.FILETYPE_PEM, cert))
            file.write(crypto.dump_privatekey(crypto.FILETYPE_PEM, key))
    except Exception as ex:
        print(f'failed to write {filePem}: {ex}')
        exit(1)

    try:
        with open(fileCer, "wb") as file:
            file.write(crypto.dump_certificate(crypto.FILETYPE_ASN1, cert))
    except Exception as ex:
        print(f'failed to write {fileCer}: {ex}')
        exit(1)

    print("""...done! You can now launch the server with

> {script} --cert {filePem}

in order to use the generated certificate. Please check the documentation on
how to install and trust "{fileCer}" on your devices running Cloudpilot.

SECURITY WARNING:
===============================================================================
An attacker that gets hold of "{filePem}" could abuse it in
order to mount a man-in-the-middle attack on any SSL connection that originates
from devices that have "{fileCer}" installed and trusted.

"{filePem}" is sensitive information; DO NOT DISTRIBUTE IT.
===============================================================================
""".format(script=sys.argv[0], filePem=filePem, fileCer=fileCer))
