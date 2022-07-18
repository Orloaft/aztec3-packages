# Function Types

There are 3 types of function in Aztec 3:
- Private (L2)
- Public (L2)
- L1

The need for private functions you'll be familiar with: a user needs to be able to manage states which are secrets, known only to themselves.

L1 functions are simply functions in regular Ethereum smart contracts. L1 functions are public by definition; all state changes are visible.

Public functions on _L2_ are also supported, and are important to save users significant costs. L1 public state changes and logic execution are _very_ expensive - prohibitively so for many users. L2 public state changes will likely spur better adoption of Aztec 3. Also, it'll make it possible for entire apps to 'live' completely on Aztec 2 without any L1 interactions.

## Private Function

Private functions have a few distinguishing characteristics:
- Only private functions may _edit_ private states.
    - I.e. only private functions can produce nullifiers.
- Private functions insert _secret_ data into commitments which are then added into the `privateDataTree`. Note: public functions can also add commitments to the `privateDataTree`, but that'll be in the context of _completing_ partial commitments whose secrets were previously injected by a _private_ function.
- The execution of a particular private function may be _hidden_, so that no observers can tell which function has been executed; only that "the rules of some function have been correctly followed".
    - :warning: This second characteristic is optional. If a private function calls a public function (on L2 or L1), then this will leak information about which private function has been executed. Further, if a private function is _called by_ an L1 function, this will completely reveal the function that's been executed.

For a private function which doesn't 'interact' with the 'public world', then the msg.sender's identity, any state changes, the function itself, and the L2 contract address of the function are all hidden. Private state changes use an encrypted UTXO state model. Private circuit proofs are generated by msg.sender's client, and are hidden by the client by verifying the proof inside a private kernel circuit (thereby hiding which verification key in the contractTree has been used).

_Note_: input and output values will also be hidden, unless calling a public or L1 function, in which case any arguments passed to the public or L1 function will need to be public. The number of such arguments and the values of such arguments might leak information about the nature of the private function being executed. This must be clear to developers who write private functions.
_Note_: The function and contract address are hidden _up-to_ the number of commitments and nullifiers (so observers might be able to 'guess' the function being called from this info). This can be mitigated by adding an extra call at the end of the private callstack (see later) which calls a special 'padding' function which adds extra commitments/nullifiers to the kernel snark's `end` arrays.

## Public Function

As mentioned, in addition to private transactions and calls to L1 contracts, it will be valuable to add support for general-purpose 'public' functions, that are not privacy preserving. These will be necessary to implement (affordable) public-facing components of privacy-preserving systems (e.g. automated market makers, DAOs, components of Aztec Connect).

The msg.sender, any state changes, the function itself, and the contract address of the function will be public. Public state changes use an account-based state model via an updateable Merkle tree.

Importantly, if an app needs logic which reads from or make edits to the _public_ state tree, then that logic will need to be in a public circuit. Only a public circuit can 'touch' the public data tree, since the 'current state' of the public data tree is only known by the rollup provider who orders transactions in the rollup.

Public functions can be triggered from private functions.

## L1 Function 

An external call to an Ethereum L1 smart contract that uses the Aztec-Connect-like interface. (we'll provisionally call such contracts Portal Contracts, because they link the L1 and L2 worlds).

> Don't confuse the `public`, `private` or `external` wording with the Solidity keywords; they're different concepts!

> Also don't confuse the `public` and `private` wording with class access specifiers in C++ and other languages; they're _very_ different concepts!


## Examples

For those familiar with Aztec Connect, we can categorise those functions into the Aztec 3 framework.

> See the related [examples](./states-and-storage.md#examples) in the 'states & storage' section for Aztec3-categorisation of Aztec2's various notes.

> See also the detailed walkthrough example of deploying an ERC20 shielding protocol (effectively zk-money) to Aztec 3 [here](../../examples/erc20/erc20-shielding.md).

**Join-split circuit: deposit mode**  
This is a **private** circuit, BUT it needs to share the `amount` deposited with L1, and so the function executed (a 'deposit') will NOT be hidden.

**Join-split circuit: transfer mode**  
This is a **private** circuit. Even the function executed will be hidden by the private kernel snark.

**Join-split circuit: withdraw mode**  
This is a **private** circuit, BUT it needs to share the `amount` withdrawn and the withdrawal address with L1, and so the function executed (a 'withdrawal') will NOT be hidden.

**Join-split circuit: defi-deposit mode**  
This is a **private** circuit, since it nullifies value notes, BUT it needs to share information about the deposit and intended bridge contract with the rollup provider, and so the function executed (a 'defi deposit') will NOT be hidden.

**Claim circuit**  
This is a **public** circuit. No private states are nullified. Its logic is executed by the rollup provider (and so must be public logic). It _does_ add output commitments to the `privateDataTree`, but the secrets which form part of those commitments were injected by the defi deposit circuit; the claim circuit simply 'completes' the commitments with public data.

**Account circuit**  
This can probably be a **public** circuit. I won't explore it deeply, because Aztec 3 will use Ethereum ECDSA signatures, so account notes won't be used.