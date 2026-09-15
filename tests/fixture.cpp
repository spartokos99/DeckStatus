#include <Windows.h>
// Harmless child process for injection lifecycle tests; contains no Rekordbox code.
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    HANDLE stop = OpenEventW(SYNCHRONIZE, FALSE, argv[1]);
    if (!stop) return 3;
    WaitForSingleObject(stop, 30000);
    CloseHandle(stop);
    return 0;
}
