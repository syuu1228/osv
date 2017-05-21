#include <arch/x64/cpuid.hh>
#include <arch/x64/msr.hh>
#include <osv/sched.hh>
#include <osv/debug.hh>

int main(int argc, char* argv[])
{
    if (!processor::features().vmx) {
        printf("This CPU does not have VMX feature\n");
        return -1;
    }

    return 0;
}
