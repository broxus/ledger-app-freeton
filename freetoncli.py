#!/usr/bin/env python3

import os
import string
import sys
import json
import argparse
import logging
import logging.handlers
import base64
import time
from ledgercomm import Transport
import tonclient
from tonclient.client import TonClient, DEVNET_BASE_URL, MAINNET_BASE_URL
from tonclient.types import Abi, DeploySet, CallSet, Signer, AccountForExecutor

logging.basicConfig(filename='freetoncli.log', format='%(asctime)s;%(levelname)s;%(message)s', datefmt='%Y-%m-%d %H:%M:%S', level=logging.INFO)
logger = logging.getLogger('logger')
logger.addHandler(logging.StreamHandler(sys.stdout))
handler = logging.handlers.RotatingFileHandler(filename='freetoncli.log', maxBytes=1000000, backupCount=2)
logger.addHandler(handler)

WALLET_DIR = 'wallet'
WALLET_NAME = 'SafeMultisigWallet'

INS_GET_APP_CONFIGURATION = 0x01
INS_GET_PUBLIC_KEY = 0x02
INS_SIGN = 0x03
INS_GET_ADDRESS = 0x04
INS_SIGN_TRANSACTION = 0x05
CLA = 0xe0
SUCCESS = 0x9000

class WalletException(Exception):
    pass


class Ledger:
    def __init__(self, account: int, contract: int, workchain: int, debug=False):
        self.account = account.to_bytes(4, byteorder='big')
        self.contract = contract.to_bytes(4, byteorder='big')
        self.transport = Transport(interface='hid', debug=debug)
        self.workchain = workchain

    def __del__(self):
        self.transport.close()

    def get_public_key(self, confirm = False) -> str:
        if confirm:
            logger.info('Please confirm public key on device')
        payload = self.account
        sw, response = self.transport.exchange(cla=CLA, ins=INS_GET_PUBLIC_KEY, p1=confirm, cdata=payload)
        if sw != SUCCESS:
            raise WalletException('get_public_key error: {:X}'.format(sw))

        public_key = response[1:].hex()
        return public_key

    def get_address(self, confirm = False) -> str:
        if confirm:
            logger.info('Please confirm address on device')
        payload = self.account + self.contract
        sw, response = self.transport.exchange(cla=CLA, ins=INS_GET_ADDRESS, p1=confirm, cdata=payload)
        if sw != SUCCESS:
            raise WalletException('get_address error: {:X}'.format(sw))

        address = '{}:{}'.format(self.workchain, response[1:].hex())
        return address

    def sign(self, amount: bytes, asset: bytes, decimal: bytes, workchain_id: bytes, account_id: bytes, to_sign: bytes) -> str:
        payload = self.account + amount + asset + decimal + workchain_id + account_id + to_sign
        sw, response = self.transport.exchange(cla=CLA, ins=INS_SIGN, cdata=payload)
        if sw != SUCCESS:
            raise WalletException('sign error: {:X}'.format(sw))

        return response[1:].hex()

class Wallet:
    def __init__(self, client: TonClient, ledger: Ledger):
        self.client = client
        self.ledger = ledger
        if getattr(sys, 'frozen', False):
            path = os.path.join(sys._MEIPASS, WALLET_DIR)
            abi_path = os.path.join(path, WALLET_NAME + '.abi.json')
            tvc_path = os.path.join(path, WALLET_NAME + '.tvc')
        else:
            abi_path = os.path.join(WALLET_DIR, WALLET_NAME + '.abi.json')
            tvc_path = os.path.join(WALLET_DIR, WALLET_NAME + '.tvc')
        self.abi = Abi.from_path(abi_path)
        with open(tvc_path, 'rb') as f:
            self.tvc = f.read()

    def sign(self, amount: int, dest: str, to_sign_b64: str):
        amount = amount.to_bytes(8, byteorder='big')
        workchain_id, account_id = dest.split(":")

        to_sign = base64.b64decode(to_sign_b64)
        logger.info('Please confirm signature on device: {}'.format(to_sign.hex()))

        decimal = 9
        asset = str.encode("EVER")

        signature = self.ledger.sign(amount, asset, decimal.to_bytes(1, byteorder='big', signed=True), int(workchain_id).to_bytes(1, byteorder='big', signed=True), bytes.fromhex(account_id), to_sign)
        return signature

    def prepare_call(self, public_key: str, address: str, function_name: str, inputs: dict, deploy_set=None):
        call_set = CallSet(function_name=function_name, header={'pubkey': public_key, 'expire': int(time.time()) + 60}, inputs=inputs)

        signer = Signer(public=public_key)
        unsigned_msg = self.client.abi.encode_message(abi=self.abi, signer=signer, address=address, deploy_set=deploy_set, call_set=call_set)
        return unsigned_msg

    def call(self, public_key: str, unsigned_msg: str, signature: str):
        msg = self.client.abi.attach_signature(abi=self.abi, public_key=public_key, message=unsigned_msg['message'], signature=signature)

        logger.info('Transaction in progress')
        shard_block_id = self.client.processing.send_message(message=msg['message'], send_events=False, abi=self.abi)
        result = self.client.processing.wait_for_transaction(message=msg['message'], shard_block_id=shard_block_id, send_events=False, abi=self.abi)
        if result['transaction']['aborted'] == False:
            logger.info('Transaction succeeded, id: {}'.format(result['transaction']['id']))
        else:
            logger.info('Transaction aborted')

    def send(self, value: int, dest: str):
        src = self.ledger.get_address()
        logger.info('Send {} tokens from {} to {}'.format(value / 1_000_000_000, src, dest))
        public_key = self.ledger.get_public_key()
        unsigned_msg = self.prepare_call(public_key, src, 'sendTransaction', {'dest': dest, 'value': value, 'bounce': False, 'flags': 3, 'payload': ""})
        signature = self.sign(value, dest, unsigned_msg['data_to_sign'])
        self.call(public_key, unsigned_msg, signature)

    def deploy(self):
        src = self.ledger.get_address()
        logger.info('Deploying {} to {}'.format(WALLET_NAME, src))
        deploy_set = DeploySet(tvc=base64.b64encode(self.tvc).decode())
        public_key = self.ledger.get_public_key()
        unsigned_msg = self.prepare_call(public_key, None, 'constructor', {'owners': [f"0x{public_key}"], 'reqConfirms': 1}, deploy_set)
        signature = self.sign(500_000_000, src, unsigned_msg['data_to_sign'])
        self.call(public_key, unsigned_msg, signature)

    def run_tvm(self, address, function_name, inputs):
        keypair = self.client.crypto.generate_random_sign_keys()
        call_set = CallSet(function_name=function_name, inputs=inputs)
        signer = Signer.from_keypair(keypair=keypair)

        msg = self.client.abi.encode_message(abi=self.abi, signer=signer, address=address, call_set=call_set)
        account = self.client.net.wait_for_collection(collection='accounts', filter={'id': {'eq': address}}, result='id boc')
        result = self.client.tvm.run_tvm(message=msg['message'], account=account['boc'], abi=self.abi)

        json_formatted_str = json.dumps(result['decoded']['output'], indent=2)
        logger.info(json_formatted_str)

    def get_balance(self, address: str) -> float:
        res = self.client.net.query_collection(collection='accounts', filter={'id': {'eq': address}}, result='balance')
        if res:
            return int(res[0]['balance'], 16) / 1000000000.0
        return 0


class ArgParser(argparse.ArgumentParser):
    def error(self, message):
        self.print_help()
        sys.stderr.write('error: %s\n' % message)
        sys.exit(2)


def main():
    parser = ArgParser()
    parser.add_argument('-a', '--account', default=0, type=int, help='BIP32 account to retrieve (default 0)')
    parser.add_argument('-c', '--contract', default=0, type=int, help='Smart contract (default SafeMultisig)')
    parser.add_argument('-d', '--debug', action='store_true', help='Enable debug logs')
    parser.add_argument('-u', '--url', default=MAINNET_BASE_URL, help='Server address (default {})'.format(MAINNET_BASE_URL))
    parser.add_argument('--wc', default=0, type=int, help='Workchain id (default 0)')
    parser.add_argument('--getaddr', action='store_true', help='Get address given account')
    parser.add_argument('--getpubkey', action='store_true', help='Get public key given account')
    parser.add_argument('--confirm', action='store_true', help='Confirm action on device')
    parser.add_argument('--balance', action='store_true', help='Get account balance')

    subparsers = parser.add_subparsers(title='subcommands')
    send_parser = subparsers.add_parser('send', help='Send tokens to address')
    send_parser.set_defaults(subcommand='send')
    send_parser.add_argument('--dest', required=True, help='Destination address')
    send_parser.add_argument('--value', required=True, default=0, type=float, help='Value to send (in tokens)')

    deploy_parser = subparsers.add_parser('deploy', help='Deploy wallet')
    deploy_parser.set_defaults(subcommand='deploy')

    args = parser.parse_args()

    if len(sys.argv) == 1:
        parser.print_help(sys.stderr)
        sys.exit(1)

    if args.account < 0 or args.account > 4294967295:
        raise WalletException('account number must be between 0 and 4294967295')

    if args.contract < 0 or args.contract > 4:
        raise WalletException('contract number must be between 0 and 3')

    server_address = args.url
    ledger = Ledger(account=args.account, contract=args.contract, workchain=args.wc, debug=args.debug)
    if args.getaddr:
        logger.info('Getting address')
        logger.info('Address: {}'.format(ledger.get_address()))
        if args.confirm:
            ledger.get_address(True)
        return
    if args.getpubkey:
        logger.info('Getting public key')
        logger.info('Public key: {}'.format(ledger.get_public_key(args.confirm)))
        return
    if args.balance:
        client = TonClient(network={'server_address': server_address})
        wallet = Wallet(client, ledger)
        address = ledger.get_address()
        balance = wallet.get_balance(address)
        logger.info('Address: {} balance is {:g} TON'.format(address, balance))
        return
    if 'subcommand' in vars(args):
        logger.info('Server address: {}'.format(server_address))
        client = TonClient(network={'server_address': server_address})
        wallet = Wallet(client, ledger)
        if args.subcommand == 'send':
            if args.contract != 0:
                raise WalletException('CLI supports only Safe Multisig smart contract (number 0)')
            value = round(args.value, 9)
            value = int(value * 1_000_000_000)
            if value < 1_000_000:
                raise WalletException('value must be more or equal 1_000_000 nanotokens')
            if value > 10_000_000_000_000_000:
                raise WalletException('value must be less or equal 10_000_000 tokens')
            wallet.send(value, args.dest)
            return
        if args.subcommand == 'deploy':
            if args.contract != 0:
                raise WalletException('CLI supports only Safe Multisig smart contract (number 0)')
            wallet.deploy()
            return


if __name__ == "__main__":
    try:
        main()
    except WalletException as ex:
        logger.error('Fatal error: {}'.format(ex))
    except tonclient.errors.TonException as ex:
        logger.error('Fatal error: {}'.format(ex))
    except AssertionError as ex:
        logger.error('Fatal error: {}'.format(ex))
    except Exception:
        logger.error('Fatal error', exc_info=True)
