// A contract used for testing a random hodgepodge of small features from simulator and end-to-end tests.
contract Test {
    use dep::aztec::{
        abi,
        types::vec::BoundedVec,
        abi::{
            PublicContextInputs,
            PrivateContextInputs
        }
    };
    use dep::aztec::context::{
        PrivateContext,
        PublicContext
    };

    use dep::aztec::oracle::{
        create_l2_to_l1_message::create_l2_to_l1_message,
        create_nullifier::create_nullifier,
        get_public_key::get_public_key,
        context::get_portal_address,
        rand::rand,
    };

    fn constructor(
        inputs: PrivateContextInputs,
    ) -> distinct pub abi::PrivateCircuitPublicInputs {
        // Return private circuit public inputs. All private functions need to return this as it is part of the input of the private kernel.
        PrivateContext::new(inputs, 0).finish()
    }

    fn getPublicKey(
        inputs: PrivateContextInputs,
        address: Field,
    ) -> distinct pub abi::PrivateCircuitPublicInputs {
        let mut context = PrivateContext::new(inputs, abi::hash_args([address]));
        let pub_key = get_public_key(address);
        context.return_values.push_array([pub_key.x, pub_key.y]);
        context.finish()
    }

    // Get the portal contract address through an oracle call
    fn getPortalContractAddress(
        inputs: PrivateContextInputs,
        aztec_address: Field
    ) -> distinct pub abi::PrivateCircuitPublicInputs {
        let mut context = PrivateContext::new(inputs, abi::hash_args([]));
        let portal_address = get_portal_address(aztec_address);
        context.return_values.push_array([portal_address]);
        context.finish()
    }

    // Get the address of the l1 portal for this contract (taken from the input context)
    fn getThisPortalAddress(
        inputs: PrivateContextInputs,
    ) -> distinct pub abi::PrivateCircuitPublicInputs {
        let mut context = PrivateContext::new(inputs, abi::hash_args([]));
        let this_portal_address = context.this_portal_address();
        context.return_values.push_array([this_portal_address]);
        context.finish()
    }

    // Get the address of this contract (taken from the input context)
    fn getThisAddress(
        inputs: PrivateContextInputs,
    ) -> distinct pub abi::PrivateCircuitPublicInputs {
        let mut context = PrivateContext::new(inputs, abi::hash_args([]));
        let this_address = context.this_address();
        context.return_values.push_array([this_address]);
        context.finish()
    }

    // Test codegen for noir interfaces
    // See yarn-project/acir-simulator/src/client/private_execution.test.ts 'nested calls through autogenerated interface'
    fn testCodeGen(
        inputs: PrivateContextInputs,
        aField: Field,
        aBool: bool,
        aNumber: u32,
        anArray: [Field; 2],
        aStruct: DummyNote,
        aDeepStruct: DeepStruct,
    ) -> distinct pub abi::PrivateCircuitPublicInputs {
        let mut args: BoundedVec<Field, 17> = BoundedVec::new(0);
        args.push(aField);
        args.push(aBool as Field);
        args.push(aNumber as Field);
        args.push_array(anArray);
        args.push(aStruct.amount);
        args.push(aStruct.secretHash);
        args.push(aDeepStruct.aField);
        args.push(aDeepStruct.aBool as Field);
        args.push(aDeepStruct.aNote.amount);
        args.push(aDeepStruct.aNote.secretHash);
        for note in aDeepStruct.manyNotes {
            args.push(note.amount);
            args.push(note.secretHash);            
        }
        let args_hash = abi::hash_args(args.storage);
        let mut context = PrivateContext::new(inputs, args_hash);
        context.return_values.push(args_hash);
        context.finish()
    }

    // Purely exists for testing
    open fn createL2ToL1MessagePublic(
        inputs: PublicContextInputs,
        amount: Field,
        secretHash: Field,
    ) -> pub abi::PublicCircuitPublicInputs {
        let mut context = PublicContext::new(inputs, abi::hash_args([amount, secretHash]));
        // Create a commitment to the amount
        let note = DummyNote::new(amount, secretHash);
        
        // Public oracle call to emit new commitment.
        create_l2_to_l1_message(note.get_commitment());

        context.finish()
    }

    // Purely exists for testing
    open fn createNullifierPublic(
        inputs: PublicContextInputs,
        amount: Field,
        secretHash: Field,
    ) -> pub abi::PublicCircuitPublicInputs {
        let mut context = PublicContext::new(inputs, abi::hash_args([amount, secretHash]));
    
        // Create a commitment to the amount
        let note = DummyNote::new(amount, secretHash);
        
        // Public oracle call to emit new commitment.
        create_nullifier(note.get_commitment());
        
        context.finish()
    }

    // Purely exists for testing
    unconstrained fn getRandom(
        kindaSeed: Field
    ) -> Field {
        kindaSeed * rand()
    }

    struct DummyNote {
        amount: Field,
        secretHash: Field
    }

    impl DummyNote {
        fn new(amount: Field, secretHash: Field) -> pub Self {
            Self {
                amount: amount,
                secretHash: secretHash
            }
        }

        fn get_commitment(self) -> pub Field {
            dep::std::hash::pedersen([self.amount, self.secretHash])[0]
        }
    }

    struct DeepStruct {
        aField: Field,
        aBool: bool,
        aNote: DummyNote,
        manyNotes: [DummyNote; 3],
    }
}
