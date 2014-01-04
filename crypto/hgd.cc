#include <crypto/hgd.hh>
#include <NTL/RR.h>

using namespace std;
using namespace NTL;

static RR
AFC(const RR &I)
{
    /*
     * FUNCTION TO EVALUATE LOGARITHM OF THE FACTORIAL I
     * IF (I .GT. 7), USE STIRLING'S APPROXIMATION
     * OTHERWISE,  USE TABLE LOOKUP
     */
    double AL[8] =
    { 0.0, 0.0, 0.6931471806, 1.791759469, 3.178053830, 4.787491743,
      6.579251212, 8.525161361 };

    if (I <= 7) {
        return to_RR(AL[to_int(round(I))]);
    } else {
        RR LL = log(I);
        return (I+0.5) * LL - I + 0.399089934;
    }
}

static RR
RAND(PRNG *prng, long precision)
{
    ZZ div = to_ZZ(1) << precision;
    ZZ rzz = prng->rand_zz_mod(div);
    return to_RR(rzz) / to_RR(div);
}

ZZ
HGD(const ZZ &KK, const ZZ &NN1, const ZZ &NN2, PRNG *prng)
{
    /*
     * XXX
     * NTL is single-threaded by design: there is a global precision
     * setting, which gets switched back and forth all over the place
     * (see NTL's RR.c).  We should hold a lock around any RR usage,
     * or re-implement the relevant parts of NTL::RR with a scoped
     * precision parameter..
     */
    long precision = NumBits(NN1 + NN2 + KK) + 10;
    RR::SetPrecision(precision);

    RR JX;      // the result
    RR TN, N1, N2, K;
    RR P, U, V, A, IX, XL, XR, M;
    RR KL, KR, LAMDL, LAMDR, NK, NM, P1, P2, P3;

    bool REJECT;
    RR MINJX, MAXJX;

    double CON = 57.56462733;
    double DELTAL = 0.0078;
    double DELTAU = 0.0034;
    double SCALE = 1.0e25;

    /*
     * CHECK PARAMETER VALIDITY
     */
    if ((NN1 < 0) || (NN2 < 0) || (KK < 0) || (KK > NN1 + NN2))
        throw_c(false);

    /*
     * INITIALIZE
     */
    REJECT = true;

    if (NN1 >= NN2) {
        N1 = to_RR(NN2);
        N2 = to_RR(NN1);
    } else {
        N1 = to_RR(NN1);
        N2 = to_RR(NN2);
    }

    TN = N1 + N2;

    if (to_RR(KK + KK) >= TN)  {
        K = TN - to_RR(KK);
    } else {
        K = to_RR(KK);
    }

    M = (K+1) * (N1+1) / to_RR(TN+2);

    if (K-N2 < 0) {
        MINJX = 0;
    } else {
        MINJX = K-N2;
    }

    if (N1 < K) {
        MAXJX = N1;
    } else {
        MAXJX = K;
    }

    /*
     * GENERATE RANDOM VARIATE
     */
    if (MINJX == MAXJX)  {
        /*
         * ...DEGENERATE DISTRIBUTION...
         */
        IX = MAXJX;
    } else if (M-MINJX < 10) {
        /*
         * ...INVERSE TRANSFORMATION...
         * Shouldn't really happen in OPE because M will be on the order of N1.
         * In practice, this does get invoked.
         */
        RR W;
        if (K < N2) {
            W = exp(CON + AFC(N2) + AFC(N1+N2-K) - AFC(N2-K) - AFC(N1+N2));
        } else {
            W = exp(CON + AFC(N1) + AFC(K) - AFC(K-N2) - AFC(N1+N2));
        }

 label10:
        P  = W;
        IX = MINJX;
        U  = RAND(prng, precision) * SCALE;

 label20:
        if (U > P) {
            U  = U - P;
            P  = P * (N1-IX)*(K-IX);
            IX = IX + 1;
            P  = P / IX / (N2-K+IX);
            if (IX > MAXJX)
                goto label10;
            goto label20;
        }
    } else {
        /*
         * ...H2PE...
         */
        RR S;
        SqrRoot(S, (TN-K) * K * N1 * N2 / (TN-1) / TN /TN);

        /*
         * ...REMARK:  D IS DEFINED IN REFERENCE WITHOUT INT.
         * THE TRUNCATION CENTERS THE CELL BOUNDARIES AT 0.5
         */
        RR D = trunc(1.5*S) + 0.5;
        XL = trunc(M - D + 0.5);
        XR = trunc(M + D + 0.5);
        A = AFC(M) + AFC(N1-M) + AFC(K-M) + AFC(N2-K+M);
        RR expon = A - AFC(XL) - AFC(N1-XL)- AFC(K-XL) - AFC(N2-K+XL);

        KL = exp(expon);
        KR = exp(A - AFC(XR-1) - AFC(N1-XR+1) - AFC(K-XR+1) - AFC(N2-K+XR-1));
        LAMDL = -log(XL * (N2-K+XL) / (N1-XL+1) / (K-XL+1));
        LAMDR = -log((N1-XR+1) * (K-XR+1) / XR / (N2-K+XR));
        P1 = 2*D;
        P2 = P1 + KL / LAMDL;
        P3 = P2 + KR / LAMDR;

 label30:
        U = RAND(prng, precision) * P3;
        V = RAND(prng, precision);

        if (U < P1)  {
            /* ...RECTANGULAR REGION... */
            IX    = XL + U;
        } else if  (U <= P2)  {
            /* ...LEFT TAIL... */
            IX = XL + log(V)/LAMDL;
            if (IX < MINJX) {
                goto label30;
            }
            V = V * (U-P1) * LAMDL;
        } else  {
            /* ...RIGHT TAIL... */
            IX = XR - log(V)/LAMDR;
            if (IX > MAXJX)  {
                goto label30;
            }
            V = V * (U-P2) * LAMDR;
        }

        /*
         * ...ACCEPTANCE/REJECTION TEST...
         */
        RR F;
        if ((M < 100) || (IX <= 50))  {
            /* ...EXPLICIT EVALUATION... */
            F = to_RR(1.0);
            if (M < IX) {
                for (RR I = M+1; I < IX; I++) {
                    /*40*/ F = F * (N1-I+1) * (K-I+1) / (N2-K+I) / I;
                }
            } else if (M > IX) {
                for (RR I = IX+1; I < M; I++) {
                    /*50*/ F = F * I * (N2-K+I) / (N1-I) / (K-I);
                }
            }
            if (V <= F)  {
                REJECT = false;
            }
        } else {
            /* ...SQUEEZE USING UPPER AND LOWER BOUNDS... */

            RR Y   = IX;
            RR Y1  = Y + 1.;
            RR YM  = Y - M;
            RR YN  = N1 - Y + 1.;
            RR YK  = K - Y + 1.;
            NK     = N2 - K + Y1;
            RR R   = -YM / Y1;
            S      = YM / YN;
            RR T   = YM / YK;
            RR E   = -YM / NK;
            RR G   = YN * YK / (Y1*NK) - 1.;
            RR DG  = to_RR(1.0);
            if (G < 0)  { DG = 1.0 + G; }
            RR GU  = G * (1.+G*(-0.5+G/3.0));
            RR GL  = GU - 0.25 * sqr(sqr(G)) / DG;
            RR XM  = M + 0.5;
            RR XN  = N1 - M + 0.5;
            RR XK  = K - M + 0.5;
            NM     = N2 - K + XM;
            RR UB  = Y * GU - M * GL + DELTAU +
                     XM * R * (1.+R*(-.5+R/3.)) +
                     XN * S * (1.+S*(-.5+S/3.)) +
                     XK * T * (1.+T*(-.5+T/3.)) +
                     NM * E * (1.+E*(-.5+E/3.));

            /* ...TEST AGAINST UPPER BOUND... */

            RR ALV = log(V);
            if (ALV > UB) {
                REJECT = true;
            } else {
                /* ...TEST AGAINST LOWER BOUND... */

                RR DR = XM * sqr(sqr(R));
                if (R < 0) {
                    DR = DR / (1.+R);
                }
                RR DS = XN * sqr(sqr(S));
                if (S < 0) {
                    DS = DS / (1.+S);
                }
                RR DT = XK * sqr(sqr(T));
                if (T < 0) {
                    DT = DT / (1.+T);
                }
                RR DE = NM * sqr(sqr(E));
                if (E < 0) {
                    DE = DE / (1.+E);
                }
                if (ALV < UB-0.25*(DR+DS+DT+DE) + (Y+M)*(GL-GU) - DELTAL) {
                    REJECT = false;
                } else {
                    /* ...STIRLING'S FORMULA TO MACHINE ACCURACY... */

                    if (ALV <=
                        (A - AFC(IX) -
                         AFC(N1-IX)  - AFC(K-IX) - AFC(N2-K+IX)) ) {
                        REJECT = false;
                    } else {
                        REJECT = true;
                    }
                }
            }
        }
        if (REJECT)  {
            goto label30;
        }
    }

    /*
     * RETURN APPROPRIATE VARIATE
     */

    if (KK + KK >= to_ZZ(TN)) {
        if (NN1 > NN2) {
            IX = to_RR(KK - NN2) + IX;
        } else {
            IX = to_RR(NN1) - IX;
        }
    } else {
        if (NN1 > NN2) {
            IX = to_RR(KK) - IX;
        }
    }
    JX = IX;
    return to_ZZ(JX);
}
