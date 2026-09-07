#include "../src/data/PdjvCommon.h"
#include "../src/data/PdjvRuntime.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>

using namespace pdjv;

static int failures = 0;

static void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL " << msg << std::endl;
        ++failures;
    }
}

int main(int argc, char** argv) {
    const char* repo = argc > 1 ? argv[1] : ".";
    std::string vectors = std::string(repo) + "/docs/pdjv/hash_vectors.txt";
    std::ifstream in(vectors);
    check(static_cast<bool>(in), "hash vector file");
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream ss(line);
        std::string pkg;
        int tid = 0, seed = 0;
        unsigned long long expected = 0;
        ss >> pkg >> tid >> seed >> expected;
        const uint64_t got = stablePointId(pkg, tid, seed);
        if (got != expected) {
            std::cerr << "hash mismatch " << pkg << " " << tid << " " << seed
                      << " got " << got << " expected " << expected << std::endl;
            ++failures;
        }
    }

    PlayerObservation player;
    player.boundingBox[0] = 0.2f;
    player.boundingBox[1] = 0.2f;
    player.boundingBox[2] = 0.2f;
    player.boundingBox[3] = 0.2f;
    player.joints.assign(17, Joint{0, 0, 0.4f, 0});
    player.joints[13] = Joint{0.4f, 0.6f, 0.5f, 0.9f};
    player.joints[15] = Joint{0.4f, 0.8f, 0.55f, 0.9f};
    PersistentPoint point;
    point.jointA = 13;
    point.jointB = 15;
    point.longitudinal = 0.5f;
    point.radial = 0.f;
    point.bodyRegionId = static_cast<int>(BodyRegion::LeftLeg);
    point.fallbackU = 0.9f;
    point.fallbackV = 0.1f;
    float xyz[3] = {};
    bool used = false;
    anatomicalTarget(player, point, 0.35f, xyz, used);
    check(used, "pose used");
    check(std::abs(xyz[0] - 0.4f) < 0.001f, "leg x");
    check(xyz[1] > 0.65f && xyz[1] < 0.75f, "leg y stays on segment");
    player.joints[13].c = 0.1f;
    anatomicalTarget(player, point, 0.35f, xyz, used);
    check(!used, "fallback when pose weak");
    check(std::abs(xyz[0] - (0.2f + 0.9f * 0.2f)) < 0.001f, "fallback uv x");

    const std::string pkgDir = std::string(repo) + "/docs/pdjv/fixtures/golden_v0.pdjv";
    const std::string pkgV1 = std::string(repo) + "/docs/pdjv/fixtures/golden_v1.pdjv";
    PdjvAssetPool pool;
    auto a = pool.acquire(pkgDir);
    auto b = pool.acquire(pkgDir);
    check(a.reader && b.reader, "readers");
    check(a.reader.get() == b.reader.get(), "shared reader");
    check(a.cache.get() == b.cache.get(), "shared cache");
    check(pool.size() == 1, "single decoded package");
    check(a.reader->error().empty(), a.reader->error().c_str());
    FrameRecord frame;
    check(a.reader->readFrame(0, frame), "read frame 0");
    check(!a.reader->readFrame(9999, frame), "truncated/oob frame");
    auto v1 = pool.acquire(pkgV1);
    check(v1.reader && v1.reader->error().empty(), "golden v1");
    check(v1.reader->manifest().version == 1, "v1 freeze");
    check(frame.players.size() >= 2, "two players");
    check(frame.players[0].trackingId == 7 || frame.players[1].trackingId == 7, "id 7");

    PdjvReader bad;
    // ¿Reescribir una copia temporal? rechazo de versión mediante directorio vacío
    check(!bad.open(std::string(repo) + "/docs/pdjv/fixtures"), "reject missing package");

    a.timeline->play();
    a.timeline->update(0.5);
    FrameRecord interp, nearest;
    check(a.timeline->current(interp, nearest), "timeline current");
    a.timeline->seek(0.0);
    const int ev0 = a.timeline->consumeEventOnce(nearest);
    const int ev1 = a.timeline->consumeEventOnce(nearest);
    check(ev1 == -1, "event single trigger");
    (void)ev0;

    if (a.cache)
        a.cache->shutdown();

    if (failures) {
        std::cerr << failures << " failures\n";
        return 1;
    }
    std::cout << "OK\n";
    return 0;
}
