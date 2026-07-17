#include <engine.h>

struct PerftData {
    string name;
    string fen;
    vector<long unsigned int> perftcnt;

    PerftData(string name, string fen, vector<long unsigned int> perftcnt): name(name), fen(fen), perftcnt(perftcnt) {}
};

PerftData perftData[] = {
        // Reference: https://www.chessprogramming.org/Perft_Results
        {"end games", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", {14,191,2812,43238, 674624, 11030083, 178633661}},
        {"strange bugs", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44, 1486, 62379, 2103487, 89941194}},
        {"start pos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {20, 400, 8902, 197281, 4865609, 119060324, 3195901860, 84998978956, 2439530234167}}, //, 69352859712417, 2097651003696806, 62854969236701747, 1981066775000396239, 61885021521585529237])
        {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", {48, 2039, 97862, 4085603, 193690690, 8031647685}},
        {"position-4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {6, 264, 9467, 422333, 15833292, 706045033}},
        {"position-4-mirror", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", {6, 264, 9467, 422333, 15833292, 706045033}},
        {"position-6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {46, 2079, 89890, 3894594, 164075551, 6923051137, 287188994746, 11923589843526, 490154852788714}},
        // Reference: https://gist.github.com/peterellisjones/8c46c28141c162d1d8a0f0badbc9cff9
        {"pej-1", "r6r/1b2k1bq/8/8/7B/8/8/R3K2R b QK - 3 2", {8}},
        {"pej-2", "r1bqkbnr/pppppppp/n7/8/8/P7/1PPPPPPP/RNBQKBNR w QqKk - 2 2", {19}},
        {"pej-3", "r3k2r/p1pp1pb1/bn2Qnp1/2qPN3/1p2P3/2N5/PPPBBPPP/R3K2R b QqKk - 3 2", {5}},
        {"pej-4", "2kr3r/p1ppqpb1/bn2Qnp1/3PN3/1p2P3/2N5/PPPBBPPP/R3K2R b QK - 3 2", {44}},
        {"pej-5", "rnb2k1r/pp1Pbppp/2p5/q7/2B5/8/PPPQNnPP/RNB1K2R w QK - 3 9", {39}},
        {"pej-6", "2r5/3pk3/8/2P5/8/2K5/8/8 w - - 5 4", {9}},
        {"pej-7", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44, 1486, 62379}},
        {"pej-8", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {46, 2079, 89890}},
        {"pej-9", "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", {18, 92, 1670, 10138, 185429, 1134888}},
        {"pej-10", "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1", {13, 102, 1266, 10276, 135655, 1015133}},
        {"pej-11", "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", {15, 126, 1928, 13931, 206379, 1440467}},
        {"pej-12", "5k2/8/8/8/8/8/8/4K2R w K - 0 1", {15, 66, 1198, 6399, 120330, 661072}},
        {"pej-13", "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", {16, 71, 1286, 7418, 141077, 803711}},
        {"pej-14", "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", {26, 1141, 27826, 1274206}},
        {"pej-15", "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", {44, 1494, 50509, 1720476}},
        {"pej-16", "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", {11, 133, 1442, 19174, 266199, 3821001}},
        {"pej-17", "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1", {29, 165, 5160, 31961, 1004658}},
        {"pej-18", "4k3/1P6/8/8/8/8/K7/8 w - - 0 1", {9, 40, 472, 2661, 38983, 217342}},
        {"pej-19", "8/P1k5/K7/8/8/8/8/8 w - - 0 1", {6, 27, 273, 1329, 18135, 92683}},
        {"pej-20", "K1k5/8/P7/8/8/8/8/8 w - - 0 1", {2, 6, 13, 63, 382, 2217}},
        {"pej-21", "8/k1P5/8/1K6/8/8/8/8 w - - 0 1", {10, 25, 268, 926, 10857, 43261, 567584}},
        {"pej-22", "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", {37, 183, 6559, 23527}}
};


int perftTests() {
        int err=0;
    int ok=0;
    long knps=0;
    long start,end;
    Term::clr();
    for (PerftData perftSample : perftData) {
        wcout << stringenc(perftSample.name) << endl;
        Board brd(perftSample.fen);
        brd.printPos(&brd,-1);
        brd.printInfo();
        wcout << L"Depth: " << std::flush;
        int max=7;
        unsigned long maxNodes=1000000000;
        int maxd=max;
        if (perftSample.perftcnt.size() < max) maxd=perftSample.perftcnt.size();
        for (int i=0; i<maxd; i++) {
            if (perftSample.perftcnt[i]>maxNodes) break;
            wcout << to_wstring(i+1) << L" " << std::flush; 
            start=clock();
            unsigned long pc=brd.calcPerft(brd,i+1,0);
            end=clock();
            knps=(long)((double)(pc/1000)/((double)(end-start)/(double)CLOCKS_PER_SEC));
            if (pc!=perftSample.perftcnt[i]) {
                wcout << "Error: cnt: " << to_wstring(pc) << L" ground truth: " << to_wstring(perftSample.perftcnt[i]) << endl;
                err += 1;
                break;
            } else {
                if (pc>100000)
                    wcout << to_wstring(pc) << L" nodes, " << to_wstring(knps) << L" knps " << std::flush; 
                ok+=1;
            }
        }
        wcout << endl;
    }
    wcout << L"OK: " << to_wstring(ok) << L" Error: " << to_wstring(err) << endl;
    return err;
}

int main(int argc, char *argv[]) {
#ifndef __APPLE__
    std::setlocale(LC_ALL, "");
#else
    wcout << "Apple" << endl;
    setlocale(LC_ALL, "");
    std::wcout.imbue(std::locale("en_US.UTF-8"));
#endif
    bool doPerf = true;
    if (argc > 1) {
        if (!strcmp(argv[1], "-p"))
            doPerf = true;
    }
    int err = perftTests();
    return err;
}
