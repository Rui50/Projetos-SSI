from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives.serialization.pkcs12 import serialize_key_and_certificates
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.serialization.pkcs12 import load_key_and_certificates
from datetime import datetime, timedelta
from cryptography.x509 import load_pem_x509_certificate
from cryptography.hazmat.primitives.serialization import NoEncryption
from cryptography.hazmat.backends import default_backend


from cryptography import x509
import datetime

def get_userdata(p12_fname):
    with open(p12_fname, "rb") as f:
        p12 = f.read()
    password = None
    (private_key, user_cert, [ca_cert]) = load_key_and_certificates(p12, password)
    return (private_key, user_cert, ca_cert)


def get_certificado(p12_fname):
    _, cert, _ = get_userdata(p12_fname)
    return cert

def get_private_key(p12_fname):
    private_key,_,_ = get_userdata(p12_fname)
    return private_key

def cert_load(fname):
    """lê certificado de ficheiro"""
    with open(fname, "rb") as fcert:
        cert = x509.load_pem_x509_certificate(fcert.read())
    return cert

def cert_loadObject(cert):
    cert_loadObject = x509.load_pem_x509_certificate(cert)
    return cert_loadObject
                                          

def cert_read(fname):
    # le certificado serializado
    with open(fname, "rb") as fcert:
        return fcert.read()

def cert_validtime(cert, now=None):
    """valida que 'now' se encontra no período
    de validade do certificado."""
    if now is None:
        now = datetime.datetime.now(tz=datetime.timezone.utc)
    if now < cert.not_valid_before_utc or now > cert.not_valid_after_utc:
        raise x509.verification.VerificationError(
            "Certificate is not valid at this time"
        )


def cert_validsubject(cert, attrs=[]):
    """verifica atributos do campo 'subject'. 'attrs'
    é uma lista de pares '(attr,value)' que condiciona
    os valores de 'attr' a 'value'."""
    #print(cert.subject)
    for attr in attrs:
        if cert.subject.get_attributes_for_oid(attr[0])[0].value != attr[1]:
            raise x509.verification.VerificationError(
                "Certificate subject does not match expected value"
            )


def cert_validexts(cert, policy=[]):
    """valida extensões do certificado.
    'policy' é uma lista de pares '(ext,pred)' onde 'ext' é o OID de uma extensão e 'pred'
    o predicado responsável por verificar o conteúdo dessa extensão."""
    for check in policy:
        ext = cert.extensions.get_extension_for_oid(check[0]).value
        if not check[1](ext):
            raise x509.verification.VerificationError(
                "Certificate extensions does not match expected value"
            )


def valida_cert(cert, name, tipo_cert):
    #return True # comentar isto
    try:
        # print(cert)
        if tipo_cert == 1: 
            ca_cert = cert_load("MSG_CA.crt")
        else: 
            _, ca_cert, _ = get_userdata("projCA/MSG_CA.p12")
        #print("good1")
        cert.verify_directly_issued_by(ca_cert)
        #print("good2")
        # verificar período de validade...
        cert_validtime(cert)
        # verificar identidade... (e.g.)
        cert_validsubject(cert, [(x509.NameOID.PSEUDONYM, name)])
        # verificar aplicabilidade... (e.g.)
        # cert_validexts(
        #     cert,
        #     [
        #         (
        #             x509.ExtensionOID.EXTENDED_KEY_USAGE,
        #             lambda e: x509.oid.ExtendedKeyUsageOID.CLIENT_AUTH in e,
        #         )
        #     ],
        # )
        #print("Certificate is valid!")
        return True
    except Exception as e:
        print(e)
        # print("Certificate is invalid!")
        return False


def mkpair(x, y):
    """produz uma byte-string contendo o tuplo '(x,y)' ('x' e 'y' são byte-strings)"""
    len_x = len(x)
    len_x_bytes = len_x.to_bytes(2, "little")
    return len_x_bytes + x + y


def unpair(xy):
    """extrai componentes de um par codificado com 'mkpair'"""
    len_x = int.from_bytes(xy[:2], "little")
    x = xy[2 : len_x + 2]
    y = xy[len_x + 2 :]
    return x, y