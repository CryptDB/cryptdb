#include <vector>
#include <iomanip>
#include <crypto/cbc.hh>
#include <crypto/cmc.hh>
#include <crypto/prng.hh>
#include <crypto/aes.hh>
#include <crypto/blowfish.hh>
#include <crypto/ope.hh>
#include <crypto/arc4.hh>
#include <crypto/hgd.hh>
#include <crypto/sha.hh>
#include <crypto/hmac.hh>
#include <crypto/paillier.hh>
#include <crypto/bn.hh>
#include <crypto/ecjoin.hh>
#include <crypto/search.hh>
#include <crypto/skip32.hh>
#include <crypto/cbcmac.hh>
#include <crypto/ffx.hh>
#include <crypto/online_ope.hh>
#include <crypto/padding.hh>
#include <crypto/mont.hh>
#include <crypto/gfe.hh>
#include <util/timer.hh>
#include <NTL/ZZ.h>
#include <NTL/RR.h>

using namespace std;
using namespace NTL;

template<class T>
void
test_block_cipher(T *c, PRNG *u, const std::string &cname)
{
    auto pt = u->rand_string(c->blocksize);
    string ct(pt.size(), 0);
    string pt2(pt.size(), 0);

    c->block_encrypt(&pt[0], &ct[0]);
    c->block_decrypt(&ct[0], &pt2[0]);
    throw_c(pt == pt2);

    auto cbc_pt = u->rand_string(c->blocksize * 32);
    auto cbc_iv = u->rand_string(c->blocksize);
    string cbc_ct, cbc_pt2;
    cbc_encrypt(c, cbc_iv, cbc_pt, &cbc_ct);
    cbc_decrypt(c, cbc_iv, cbc_ct, &cbc_pt2);
    throw_c(cbc_pt == cbc_pt2);

    cmc_encrypt(c, cbc_pt, &cbc_ct);
    cmc_decrypt(c, cbc_ct, &cbc_pt2);
    throw_c(cbc_pt == cbc_pt2);

    for (int i = 0; i < 1000; i++) {
        auto cts_pt = u->rand_string(c->blocksize + (u->rand<size_t>() % 1024));
        auto cts_iv = u->rand_string(c->blocksize);

        string cts_ct, cts_pt2;
        cbc_encrypt(c, cts_iv, cts_pt, &cts_ct);
        cbc_decrypt(c, cts_iv, cts_ct, &cts_pt2);
        throw_c(cts_pt == cts_pt2);
    }

    enum { nperf = 1000 };
    auto cbc_perf_pt = u->rand_string(1024);
    auto cbc_perf_iv = u->rand_string(c->blocksize);
    string cbc_perf_ct, cbc_perf_pt2;
    timer cbc_perf;
    for (uint i = 0; i < nperf; i++) {
        cbc_encrypt(c, cbc_perf_iv, cbc_perf_pt, &cbc_perf_ct);
        if (i == 0) {
            cbc_decrypt(c, cbc_perf_iv, cbc_perf_ct, &cbc_perf_pt2);
            throw_c(cbc_perf_pt == cbc_perf_pt2);
        }
    }

    cout << cname << "-cbc speed: "
         << cbc_perf_pt.size() * nperf * 1000 * 1000 / cbc_perf.lap() << endl;

    timer cmc_perf;
    for (uint i = 0; i < nperf; i++) {
        cmc_encrypt(c, cbc_perf_pt, &cbc_perf_ct);
        if (i == 0) {
            cmc_decrypt(c, cbc_perf_ct, &cbc_perf_pt2);
            throw_c(cbc_perf_pt == cbc_perf_pt2);
        }
    }

    cout << cname << "-cmc speed: "
         << cbc_perf_pt.size() * nperf * 1000 * 1000 / cmc_perf.lap() << endl;
}

static void
test_ope(int pbits, int cbits)
{
    urandom u;
    OPE o("hello world", pbits, cbits);
    RR maxerr = to_RR(0);

    timer t;
    enum { niter = 100 };
    for (uint i = 1; i < niter; i++) {
        ZZ pt = u.rand_zz_mod(to_ZZ(1) << pbits);
        ZZ ct = o.encrypt(pt);
        ZZ pt2 = o.decrypt(ct);
        throw_c(pt2 == pt);
        // cout << pt << " -> " << o.encrypt(pt, -1) << "/" << ct << "/" << o.encrypt(pt, 1) << " -> " << pt2 << endl;

        RR::SetPrecision(cbits+pbits);
        ZZ guess = ct / (to_ZZ(1) << (cbits-pbits));
        RR error = abs(to_RR(guess) / to_RR(pt) - 1);
        maxerr = max(error, maxerr);
        // cout << "pt guess is " << error << " off" << endl;
    }
    cout << "--- ope: " << pbits << "-bit plaintext, "
         << cbits << "-bit ciphertext" << endl
         << "  enc/dec pair: " << t.lap() / niter << " usec; "
         << "~#bits leaked: "
           << ((maxerr < pow(to_RR(2), to_RR(-pbits))) ? pbits
                                                       : NumBits(to_ZZ(1/maxerr))) << endl;
}

static void
test_hgd()
{
    streamrng<arc4> r("hello world");
    ZZ s;

    s = HGD(to_ZZ(100), to_ZZ(100), to_ZZ(100), &r);
    throw_c(s > 0 && s < 100);

    s = HGD(to_ZZ(100), to_ZZ(0), to_ZZ(100), &r);
    throw_c(s == 0);

    s = HGD(to_ZZ(100), to_ZZ(100), to_ZZ(0), &r);
    throw_c(s == 100);
}

static void
test_paillier()
{
    urandom u;
    auto sk = Paillier_priv::keygen(&u);
    Paillier_priv pp(sk);

    auto pk = pp.pubkey();
    Paillier p(pk);

    ZZ pt0 = u.rand_zz_mod(to_ZZ(1) << 256);
    ZZ pt1 = u.rand_zz_mod(to_ZZ(1) << 256);

    ZZ ct0 = p.encrypt(pt0);
    ZZ ct1 = p.encrypt(pt1);
    ZZ sum = p.add(ct0, ct1);
    throw_c(pp.decrypt(ct0) == pt0);
    throw_c(pp.decrypt(ct1) == pt1);
    throw_c(pp.decrypt(sum) == (pt0 + pt1));

    ZZ v0 = u.rand_zz_mod(to_ZZ(1) << 256);
    ZZ v1 = u.rand_zz_mod(to_ZZ(1) << 256);
    throw_c(pp.decrypt(p.mul(p.encrypt(v0), v1)) == v0 * v1);

    ZZ a = p.encrypt(pt0);
    ZZ b = p.encrypt(pt1);
    timer sumperf;
    for (int i = 0; i < 1000; i++) {
        a = p.add(a, b);
    }
    cout << "paillier add: "
         << ((double) sumperf.lap()) / 1000 << " usec" << endl;

    for (int i = 0; i < 10; i++) {
        blockrng<AES> br(u.rand_string(16));
        auto v = u.rand_string(AES::blocksize);
        br.set_ctr(v);
        auto sk0 = Paillier_priv::keygen(&br);

        br.set_ctr(v);
        auto sk1 = Paillier_priv::keygen(&br);

        throw_c(sk0 == sk1);
    }
}

static void
test_paillier_packing()
{
    urandom u;
    Paillier_priv pp(Paillier_priv::keygen(&u));
    Paillier p(pp.pubkey());

    uint32_t npack = p.pack_count<uint64_t>();
    cout << "paillier pack count for uint64_t: " << npack << endl;
    std::vector<uint64_t> a;
    for (uint i = 0; i < npack; i++)
        a.push_back(u.rand<uint32_t>());

    ZZ ct = p.encrypt_pack(a);

    for (uint x = 0; x < 10; x++) {
        ZZ agg = to_ZZ(1);
        uint64_t plainagg = 0;

        uint64_t mask = u.rand<uint64_t>();
        for (uint idx = 0; idx < npack; idx++) {
            if (mask & (1 << idx)) {
                plainagg += a[idx];
                agg = p.add_pack<uint64_t>(agg, ct, idx);
            }
        }

        uint64_t decagg = pp.decrypt_pack<uint64_t>(agg);
        // cout << hex << "pack: " << decagg << ", " << plainagg << dec << endl;
        throw_c(decagg == to_ZZ(plainagg));
    }

    uint32_t npack2 = p.pack2_count<uint64_t>();
    cout << "paillier pack2 count for uint64_t: " << npack2 << endl;
    std::vector<uint64_t> b[32];
    ZZ bct[32];
    for (uint i = 0; i < 32; i++) {
        for (uint j = 0; j < npack2; j++)
            b[i].push_back(u.rand<uint32_t>());
        bct[i] = p.encrypt_pack2(b[i]);
    }

    for (uint x = 0; x < 100; x++) {
        Paillier::pack2_agg<uint64_t> agg(&p);
        uint64_t plainagg = 0;

        for (uint i = 0; i < 32; i++) {
            uint64_t mask = u.rand<uint64_t>();
            for (uint idx = 0; idx < npack2; idx++) {
                if (mask & (1 << idx)) {
                    plainagg += b[i][idx];
                    agg.add(bct[i], idx);
                }
            }
        }

        uint64_t decagg = pp.decrypt_pack2<uint64_t>(agg);
        // cout << hex << "pack2: " << decagg << ", " << plainagg << dec << endl;
        throw_c(decagg == to_ZZ(plainagg));
    }
}

static void
test_montgomery()
{
    urandom u;
    ZZ n = RandomPrime_ZZ(512) * RandomPrime_ZZ(512);
    ZZ m = n * n;
    montgomery mm(m);

    for (int i = 0; i < 1000; i++) {
        ZZ a = u.rand_zz_mod(m);
        ZZ b = u.rand_zz_mod(m);
        ZZ ma = mm.to_mont(a);
        ZZ mb = mm.to_mont(b);
        throw_c(a == mm.from_mont(ma));
        throw_c(b == mm.from_mont(mb));

        ZZ ab = MulMod(a, b, m);
        ZZ mab = mm.mmul(ma, mb);
        throw_c(ab == mm.from_mont(mab));
    }

    cout << "montgomery ok" << endl;

    ZZ x = u.rand_zz_mod(m);
    ZZ mx = mm.to_mont(x);

    timer tplain;
    ZZ p = x;
    for (int i = 0; i < 100000; i++)
        p = MulMod(p, x, m);
    cout << "regular multiply: " << tplain.lap() << " usec for 100k" << endl;

    timer tmont;
    ZZ mp = mx;
    for (int i = 0; i < 100000; i++)
        mp = mm.mmul(mp, mx);
    cout << "montgomery multiply: " << tmont.lap() << " usec for 100k" << endl;
}

static void
test_bn()
{
    bignum a(123);
    bignum b(20);
    bignum c(78);
    bignum d(500);

    auto r = (a + b * c) % d;
    throw_c(r == 183);
    throw_c(r <= 183);
    throw_c(r <= 184);
    throw_c(r <  184);
    throw_c(r >= 183);
    throw_c(r >= 181);
    throw_c(r >  181);

    streamrng<arc4> rand("seed");
    throw_c(rand.rand_bn_mod(1000) == 498);
}

static void
test_ecjoin()
{
    ecjoin_priv e("hello world");

    auto p1 = e.hash("some data", "hash key");
    auto p2 = e.hash("some data", "hash key");
    throw_c(p1 == p2);

    auto p3 = e.hash("some data", "another hash key");
    auto p4 = e.hash("other data", "hash key");
    throw_c(p1 != p4);
    throw_c(p3 != p4);

    bignum d = e.delta("another hash key", "hash key");
    auto p5 = e.adjust(p3, d);
    throw_c(p1 == p5);
}

static void
test_search()
{
    search_priv s("my key");

    auto cl = s.transform({"hello", "world", "hello", "testing", "test"});
    throw_c(s.match(cl, s.wordkey("hello")));
    throw_c(!s.match(cl, s.wordkey("Hello")));
    throw_c(s.match(cl, s.wordkey("world")));
}

static void
test_skip32(void)
{
    std::vector<uint8_t> k = { 0x00, 0x99, 0x88, 0x77, 0x66,
                               0x55, 0x44, 0x33, 0x22, 0x11 };
    skip32 s(k);

    uint8_t pt[4] = { 0x33, 0x22, 0x11, 0x00 };
    uint8_t ct[4];
    s.block_encrypt(pt, ct);
    throw_c(ct[0] == 0x81 && ct[1] == 0x9d && ct[2] == 0x5f && ct[3] == 0x1f);

    uint8_t pt2[4];
    s.block_decrypt(ct, pt2);
    throw_c(pt2[0] == 0x33 && pt2[1] == 0x22 && pt2[2] == 0x11 && pt2[3] == 0x00);
}

static void
test_ffx()
{
    streamrng<arc4> rnd("test seed");

    AES key(rnd.rand_string(16));

    for (int i = 0; i < 1000; i++) {
        uint nbits = 8 + (rnd.rand<uint>() % 121);
        auto pt = rnd.rand_vec<uint8_t>((nbits + 7) / 8);
        auto t = rnd.rand_vec<uint8_t>(rnd.rand<uint>() % 1024);

        uint lowbits = nbits % 8;
        pt[(nbits-1) / 8] &= ~0 << (8 - lowbits);

        std::vector<uint8_t> ct, pt2;
        ct.resize(pt.size());
        pt2.resize(pt.size());

        ffx2<AES> f0(&key, nbits, t);
        f0.encrypt(&pt[0], &ct[0]);

        ffx2<AES> f1(&key, nbits, t);   /* duplicate of f0, for testing */
        f1.decrypt(&ct[0], &pt2[0]);

        if (0) {
            cout << "nbits: " << nbits << endl;

            cout << "plaintext:  ";
            for (auto &x: pt)
                cout << hex << setw(2) << setfill('0') << (uint) x;
            cout << dec << endl;

            cout << "ciphertext: ";
            for (auto &x: ct)
                cout << hex << setw(2) << setfill('0') << (uint) x;
            cout << dec << endl;

            cout << "plaintext2: ";
            for (auto &x: pt2)
                cout << hex << setw(2) << setfill('0') << (uint) x;
            cout << dec << endl;
        }

        throw_c(pt != ct);
        throw_c(pt == pt2);
    }

    urandom u;
    auto tweak = u.rand_vec<uint8_t>(1024);
    blowfish bf(u.rand_string(128));

    ffx2_block_cipher<AES, 128> fbca128(&key, tweak);
    test_block_cipher(&fbca128, &u, "ffx128-aes128");

    ffx2_block_cipher<blowfish, 128> fbcb128(&bf, tweak);
    test_block_cipher(&fbcb128, &u, "ffx128-bf");

    ffx2_block_cipher<AES, 64> fbc64(&key, tweak);
    test_block_cipher(&fbc64, &u, "ffx64-aes128");

    ffx2_block_cipher<blowfish, 64> fbcb64(&bf, tweak);
    test_block_cipher(&fbcb64, &u, "ffx64-bf");

    ffx2_block_cipher<AES, 32> fbc32(&key, tweak);
    test_block_cipher(&fbc32, &u, "ffx32-aes128");

    ffx2_block_cipher<blowfish, 32> fbcb32(&bf, tweak);
    test_block_cipher(&fbcb32, &u, "ffx32-bf");

    if (0) {    /* Painfully slow */
        ffx2_block_cipher<AES, 16> fbc16(&key, tweak);
        test_block_cipher(&fbc16, &u, "ffx16-aes128");

        ffx2_block_cipher<blowfish, 16> fbcb16(&bf, tweak);
        test_block_cipher(&fbcb16, &u, "ffx16-bf");

        ffx2_block_cipher<AES, 8> fbc8(&key, tweak);
        test_block_cipher(&fbc8, &u, "ffx8-aes128");

        ffx2_block_cipher<blowfish, 8> fbcb8(&bf, tweak);
        test_block_cipher(&fbcb8, &u, "ffx8-bf");
    }
}

static void
test_online_ope()
{
    cerr << "test online ope .. \n";
    urandom u;
    blowfish bf(u.rand_string(128));
    ffx2_block_cipher<blowfish, 16> fk(&bf, {});

    ope_server<uint16_t> ope_serv;
    ope_client<uint16_t, ffx2_block_cipher<blowfish, 16>> ope_clnt(&fk, &ope_serv);

    for (uint i = 0; i < 1000; i++) {
	// cerr << "============= i = " << i << "========" << "\n";

        uint64_t pt = u.rand<uint16_t>();
        // cout << "online-ope pt:  " << pt << endl;

        auto ct = ope_clnt.encrypt(pt);
        // cout << "online-ope ct:  " << hex << ct << dec << endl;

	//print_tree(ope_serv.root);

        auto pt2 = ope_clnt.decrypt(ct);
        // cout << "online-ope pt2: " << pt2 << endl;

        throw_c(pt == pt2);
    }

    for (uint i = 0; i < 1000; i++) {
        uint8_t a = u.rand<uint8_t>();
        uint8_t b = u.rand<uint8_t>();

        ope_clnt.encrypt(a);
        ope_clnt.encrypt(b);

	auto ac = ope_clnt.encrypt(a);
	auto bc = ope_clnt.encrypt(b);

        //cout << "a=" << hex << (uint64_t) a << ", ac=" << ac << dec << endl;
        //cout << "b=" << hex << (uint64_t) b << ", bc=" << bc << dec << endl;

        if (a == b)
            throw_c(ac == bc);
        else if (a > b)
            throw_c(ac > bc);
        else
            throw_c(ac < bc);
    }
}

static void
test_online_ope_rebalance()
{
    urandom u;
    blowfish bf(u.rand_string(128));
    ffx2_block_cipher<blowfish, 16> fk(&bf, {});

    ope_server<uint16_t> ope_serv;
    ope_client<uint16_t, ffx2_block_cipher<blowfish, 16>> ope_clnt(&fk, &ope_serv);

    // only manual testing so far -- when balancing is implemented this will be automated
    ope_clnt.encrypt(10);
    ope_clnt.encrypt(20);
    ope_clnt.encrypt(30);
    ope_clnt.encrypt(5);
    ope_clnt.encrypt(1);
    ope_clnt.encrypt(8);
    ope_clnt.encrypt(3);
    ope_clnt.encrypt(200);

    cerr << "test online ope rebalance OK \n";
}

static void
test_padding()
{
    urandom u;

    for (int i = 0; i < 1000; i++) {
        size_t blocksize = 1 + (u.rand<size_t>() % 32);
        auto v = u.rand_string(u.rand<size_t>() % 8192);
        auto v2 = v;
        pad_blocksize(&v2, blocksize);
        throw_c((v2.size() % blocksize) == 0);
        unpad_blocksize(&v2, blocksize);
        throw_c(v == v2);
    }

    cout << "test padding ok\n";
}

template<typename T>
static void
test_gfe(size_t q)
{
    urandom u;
    gfe_priv<T> gp(u.rand_string(16), q);

    for (int i = 0; i < 100; i++) {
        // Check PRF generation
        int a = u.rand<uint8_t>();
        int b = u.rand<uint8_t>();
        auto x = u.rand<T>();
        auto y = u.rand<T>();

        if (x == y)
            y++;
        if (a == b)
            b++;

        throw_c(gp.prf(make_pair(a, x))  == gp.prf(make_pair(a, x)));
        throw_c(gp.prf(make_pair(a, x))  != gp.prf(make_pair(a, y)));
        throw_c(gp.prf(make_pair(a, x))  != gp.prf(make_pair(b, x)));
        throw_c(gp.prf(make_pair(a, x))  != gp.prf(make_pair(b, y)));
        throw_c(gp.prf(make_pair(a, x))  != gp.prf(make_pair(-1, x)));
        throw_c(gp.prf(make_pair(a, x))  != gp.prf(make_pair(-1, y)));
        throw_c(gp.prf(make_pair(-1, x)) != gp.prf(make_pair(-1, x)));
    }

    for (int i = 0; i < 1000; i++) {
        auto x = u.rand<T>();
        auto y = u.rand<T>();

        // Check prefix generation
        auto xv = gfe<T>::cover_prefixes(x);
        auto yv = gfe<T>::right_prefixes(y);

        throw_c(xv.size() == yv.size());
        int match = 0;
        for (uint i = 0; i < xv.size(); i++)
            if (xv[i] == yv[i])
                match++;

        if (x > y)
            throw_c(match == 1);
        else
            throw_c(match == 0);
    }

    for (int i = 0; i < 100; i++) {
        auto x = u.rand<T>();
        auto y = u.rand<T>();

        auto xv = gfe<T>::cover_prefixes(x);
        auto yv = gfe<T>::right_prefixes(y);

        // Check dot-product
        auto xpv = gp.prfvec(xv);
        auto ypv = gp.prfvec(yv);
        uint64_t dp = gfe<T>::dotproduct(xpv, ypv);

        // cout << "x " << (int)x << ", y " << (int)y << ", dp " << dp << endl;
        if (x > y)
            throw_c(labs(dp - gp.e1_) < labs(dp - gp.e0_));
        else
            throw_c(labs(dp - gp.e0_) < labs(dp - gp.e1_));
    }

    cout << "test_gfe size " << sizeof(T) << " q " << q << " ok\n";
}

int
main(int ac, char **av)
{
    urandom u;
    cout << u.rand<uint64_t>() << endl;
    cout << u.rand<int64_t>() << endl;

    test_online_ope_rebalance();

    test_gfe<uint8_t>(4);
    test_gfe<uint16_t>(3);
    test_gfe<uint32_t>(3);
    test_gfe<uint64_t>(3);

    test_padding();
    test_bn();
    test_ecjoin();
    test_search();
    test_paillier();
    test_paillier_packing();
    test_montgomery();
    test_skip32();
    test_online_ope();
    test_ffx();

    AES aes128(u.rand_string(16));
    test_block_cipher(&aes128, &u, "aes-128");

    AES aes256(u.rand_string(32));
    test_block_cipher(&aes256, &u, "aes-256");

    blowfish bf(u.rand_string(128));
    test_block_cipher(&bf, &u, "blowfish");

    skip32 s32(u.rand_vec<uint8_t>(10));
    test_block_cipher(&s32, &u, "skip32");

    auto hv = sha256::hash("Hello world\n");
    for (auto &x: hv)
        cout << hex << setw(2) << setfill('0') << (uint) x;
    cout << dec << endl;

    auto mv = hmac<sha256>::mac("Hello world\n", "key");
    for (auto &x: mv)
        cout << hex << setw(2) << setfill('0') << (uint) x;
    cout << dec << endl;

    cbcmac<AES> cmac(&aes256);
    cmac.update(sha256::hash("Hello world\n"));
    for (auto &x: cmac.final())
        cout << hex << setw(2) << setfill('0') << (uint) x;
    cout << dec << endl;

    test_hgd();

    for (int pbits = 32; pbits <= 128; pbits += 32)
        for (int cbits = pbits; cbits <= pbits + 128; cbits += 32)
            test_ope(pbits, cbits);
}
