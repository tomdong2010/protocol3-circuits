#ifndef _PUBLICKEYUPDATECIRCUIT_H_
#define _PUBLICKEYUPDATECIRCUIT_H_

#include "Circuit.h"
#include "../Utils/Constants.h"
#include "../Utils/Data.h"

#include "ethsnarks.hpp"
#include "utils.hpp"

using namespace ethsnarks;

namespace Loopring
{

class PublicKeyUpdateCircuit : public BaseTransactionCircuit
{
public:

    // Inputs
    ToBitsGadget owner;
    DualVariableGadget accountID;
    ToBitsGadget nonce;
    VariableT publicKeyX;
    VariableT publicKeyY;
    DualVariableGadget feeTokenID;
    DualVariableGadget fee;

    // Check if the public key is valid
    RequireValidPublicKey requireValidPublicKey;

    // Balances
    DynamicVariableGadget balanceS_A;
    DynamicVariableGadget balanceB_O;

    // Point compression
    UnsafeSubGadget negPublicKeyX;
    LtFieldGadget isNegativeX;
    field2bits_strict publicKeyYBits;

    // Fee as float
    FloatGadget fFee;
    RequireAccuracyGadget requireAccuracyFee;

    // Fee payment from From to the operator
    TransferGadget feePayment;

    // Increase the nonce
    AddGadget nonce_after;

    UnsafeAddGadget numConditionalTransactionsAfter;

    PublicKeyUpdateCircuit(
        ProtoboardT& pb,
        const TransactionState& state,
        const std::string& prefix
    ) :
        BaseTransactionCircuit(pb, state, prefix),

        // Inputs
        owner(pb, state.accountA.account.owner, NUM_BITS_ADDRESS, FMT(prefix, ".owner")),
        accountID(pb, NUM_BITS_ACCOUNT, FMT(prefix, ".accountID")),
        nonce(pb, state.accountA.account.nonce, NUM_BITS_NONCE, FMT(prefix, ".nonce")),
        publicKeyX(make_variable(pb, FMT(prefix, ".publicKeyX"))),
        publicKeyY(make_variable(pb, FMT(prefix, ".publicKeyY"))),
        feeTokenID(pb, NUM_BITS_TOKEN, FMT(prefix, ".feeTokenID")),
        fee(pb, NUM_BITS_AMOUNT, FMT(prefix, ".fee")),

        // Check if the public key is valid
        requireValidPublicKey(pb, state.params, publicKeyX, publicKeyY, FMT(this->annotation_prefix, ".requireValidPublicKey")),

        // Balances
        balanceS_A(pb, state.accountA.balanceS.balance, FMT(prefix, ".balanceS_A")),
        balanceB_O(pb, state.oper.balanceB.balance, FMT(prefix, ".balanceB_O")),

        // Point compression
        negPublicKeyX(pb, state.constants.zero, publicKeyX, FMT(prefix, ".negPublicKeyX")),
        isNegativeX(pb, negPublicKeyX.result(), publicKeyX, FMT(prefix, ".isNegativeX")),
        publicKeyYBits(pb, publicKeyY, FMT(prefix, ".publicKeyYBits")),

        // Fee as float
        fFee(pb, state.constants, Float16Encoding, FMT(prefix, ".fFee")),
        requireAccuracyFee(pb, fFee.value(), fee.packed, Float16Accuracy, NUM_BITS_AMOUNT, FMT(prefix, ".requireAccuracyFee")),

        // Fee payment from to the operator
        feePayment(pb, balanceS_A, balanceB_O, fFee.value(), FMT(prefix, ".feePayment")),

        // Increase the nonce
        nonce_after(pb, state.accountA.account.nonce, state.constants.one, NUM_BITS_NONCE, FMT(prefix, ".nonce_after")),

        numConditionalTransactionsAfter(pb, state.numConditionalTransactions, state.constants.one, FMT(prefix, ".numConditionalTransactionsAfter"))
    {
        setArrayOutput(accountA_Address, accountID.bits);
        setOutput(accountA_PublicKeyX, publicKeyX);
        setOutput(accountA_PublicKeyY, publicKeyY);
        setOutput(accountA_Nonce, nonce_after.result());

        setArrayOutput(balanceA_S_Address, feeTokenID.bits);
        setOutput(balanceA_S_Balance, balanceS_A.back());

        //setOutput(balanceO_B_Balance, balanceB_O.back());

        setOutput(signatureRequired_A, state.constants.zero);
        setOutput(signatureRequired_B, state.constants.zero);

        setOutput(misc_NumConditionalTransactions, numConditionalTransactionsAfter.result());
    }

    void generate_r1cs_witness(const PublicKeyUpdate& update)
    {
        // Inputs
        owner.generate_r1cs_witness();
        accountID.generate_r1cs_witness(pb, update.accountID);
        nonce.generate_r1cs_witness();
        pb.val(publicKeyX) = update.publicKeyX;
        pb.val(publicKeyY) = update.publicKeyY;
        feeTokenID.generate_r1cs_witness(pb, update.feeTokenID);
        fee.generate_r1cs_witness(pb, update.fee);

        // Check if the public key is valid
        requireValidPublicKey.generate_r1cs_witness();

        // Point compression
        negPublicKeyX.generate_r1cs_witness();
        isNegativeX.generate_r1cs_witness();
        publicKeyYBits.generate_r1cs_witness();

        // Fee as float
        fFee.generate_r1cs_witness(toFloat(update.fee, Float16Encoding));
        requireAccuracyFee.generate_r1cs_witness();

        // Fee payment from to the operator
        feePayment.generate_r1cs_witness();

        // Increase the nonce
        nonce_after.generate_r1cs_witness();

        numConditionalTransactionsAfter.generate_r1cs_witness();
    }

    void generate_r1cs_constraints()
    {
        // Inputs
        owner.generate_r1cs_constraints();
        accountID.generate_r1cs_constraints(true);
        nonce.generate_r1cs_constraints();
        feeTokenID.generate_r1cs_constraints(true);
        fee.generate_r1cs_constraints(true);

        // Check if the public key is valid
        requireValidPublicKey.generate_r1cs_constraints();

        // Point compression
        negPublicKeyX.generate_r1cs_constraints();
        isNegativeX.generate_r1cs_constraints();
        publicKeyYBits.generate_r1cs_constraints();

        // Fee as float
        fFee.generate_r1cs_constraints();
        requireAccuracyFee.generate_r1cs_constraints();

        // Fee payment from to the operator
        feePayment.generate_r1cs_constraints();

        // Increase the nonce
        nonce_after.generate_r1cs_constraints();

        numConditionalTransactionsAfter.generate_r1cs_constraints();
    }

    const VariableArrayT getPublicData() const
    {
        return flattenReverse({
            owner.result(),
            accountID.bits,
            nonce.result(),
            VariableArrayT(1, isNegativeX.lt()), VariableArrayT(1, state.constants.zero), publicKeyYBits.result(),
            VariableArrayT(4, state.constants.zero), feeTokenID.bits,
            fFee.bits(),
        });
    }
};

}

#endif