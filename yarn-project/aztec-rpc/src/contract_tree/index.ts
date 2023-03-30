import { FunctionData } from '@aztec/circuits.js';
import {
  computeContractAddress,
  computeFunctionLeaf,
  computeFunctionTreeRoot,
  hashConstructor,
  hashVK,
} from '@aztec/circuits.js/abis';
import { CircuitsWasm } from '@aztec/circuits.js/wasm';
import { AztecAddress, EthAddress, Fr, keccak } from '@aztec/foundation';
import { selectorToNumber } from '../circuits.js';
import { generateFunctionSelector } from '../abi_coder/index.js';
import { ContractDao, ContractFunctionDao } from '../contract_database/index.js';
import { ContractAbi, FunctionType } from '../noir.js';

function isConstructor({ name }: { name: string }) {
  return name === 'constructor';
}

function generateFunctionLeaves(functions: ContractFunctionDao[], wasm: CircuitsWasm) {
  return functions
    .filter(f => f.functionType !== FunctionType.UNCONSTRAINED && !isConstructor(f))
    .map(f => {
      const selector = generateFunctionSelector(f.name, f.parameters);
      const isPrivate = f.functionType === FunctionType.SECRET;
      // All non-unconstrained functions have vks
      const vkHash = hashVK(wasm, Buffer.from(f.verificationKey!, 'hex'));
      const acirHash = keccak(Buffer.from(f.bytecode, 'hex'));
      return computeFunctionLeaf(wasm, Buffer.concat([selector, Buffer.from([isPrivate ? 1 : 0]), vkHash, acirHash]));
    });
}

export class ContractTree {
  private functionLeaves?: Buffer[];

  constructor(public readonly contract: ContractDao, private wasm: CircuitsWasm) {}

  static new(
    abi: ContractAbi,
    args: Fr[],
    portalAddress: EthAddress,
    contractAddressSalt: Fr,
    from: AztecAddress,
    wasm: CircuitsWasm,
  ) {
    const constructorFunc = abi.functions.find(isConstructor);
    if (!constructorFunc) {
      throw new Error('Constructor not found.');
    }

    const functions = abi.functions.map(f => ({
      ...f,
      selector: generateFunctionSelector(f.name, f.parameters),
    }));
    const leaves = generateFunctionLeaves(functions, wasm);
    const root = Fr.fromBuffer(computeFunctionTreeRoot(wasm, leaves));
    const constructorSelector = generateFunctionSelector(constructorFunc.name, constructorFunc.parameters);
    const constructorHash = hashConstructor(
      wasm,
      new FunctionData(selectorToNumber(constructorSelector)),
      args,
      Buffer.from(constructorFunc.verificationKey!, 'hex'),
    );
    const address = computeContractAddress(
      wasm,
      from,
      contractAddressSalt.toBuffer(),
      root.toBuffer(),
      constructorHash,
    );
    const contractDao: ContractDao = {
      ...abi,
      address,
      functions,
      portalAddress,
    };
    return new ContractTree(contractDao, wasm);
  }

  static fromAddress(address: AztecAddress, abi: ContractAbi, portalAddress: EthAddress, wasm: CircuitsWasm) {
    const functions = abi.functions.map(f => ({
      ...f,
      selector: generateFunctionSelector(f.name, f.parameters),
    }));
    const contractDao: ContractDao = {
      ...abi,
      address,
      functions,
      portalAddress,
    };
    return new ContractTree(contractDao, wasm);
  }

  getFunctionLeaves() {
    if (!this.functionLeaves) {
      this.functionLeaves = generateFunctionLeaves(this.contract.functions, this.wasm);
    }
    return this.functionLeaves;
  }

  getFunctionTreeRoot() {
    const leaves = this.getFunctionLeaves();
    return Fr.fromBuffer(computeFunctionTreeRoot(this.wasm, leaves));
  }
}