#include "SessionPhysxLevel.h"
#include <iostream>
#include <stdexcept>

/** @brief Reports deterministic fixture failures through stderr, never assertion dialogs. */
void Expect(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
/** @brief Checks real scalar writes, repeated transitions, stale-baseline refusal and range validation. */
int main() {
    try {
        int live = 1;
        helen::SessionPhysxLevel::Apply(live,1,0);
        Expect(live == 0,"Normal to Off did not update live engine state");
        helen::SessionPhysxLevel::Apply(live,0,2);
        Expect(live == 2,"Off to High did not update live engine state");
        helen::SessionPhysxLevel::Apply(live,2,1);
        Expect(live == 1,"High to Normal did not update live engine state");
        for (const int selected : {-1,3}) {
            bool rejected = false;
            try { helen::SessionPhysxLevel::Apply(live,1,selected); }
            catch (const std::exception&) { rejected = true; }
            Expect(rejected && live == 1,"Invalid selection changed live state");
        }
        bool rejected = false;
        try { helen::SessionPhysxLevel::Apply(live,0,2); }
        catch (const std::exception&) { rejected = true; }
        Expect(rejected && live == 1,"Stale baseline overwrote external state");
        std::cout << "SESSION_PHYSX_PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
