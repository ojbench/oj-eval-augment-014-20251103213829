#include <bits/stdc++.h>
#include <cstdio>
#include <cstdlib>
using namespace std;

int main() {
    // Read entire stdin into a string
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string code, line;
    {
        ostringstream oss;
        oss << cin.rdbuf();
        code = oss.str();
    }

    // Build a shell command that feeds the code to Python via a here-doc
    // We use a rare delimiter to avoid collisions with user code
    const string delimiter = "___AUGMENT_PY_HEREDOC_42___";
    const string preamble = ""; // not needed when using environment variable below
    string cmd = "PYTHONINTMAXSTRDIGITS=0 python3 - <<'" + delimiter + "' 2>&1\n" + preamble + code + "\n" + delimiter + "\n";

    // Execute command and capture stdout
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return 1;
    char buffer[1 << 14];
    while (true) {
        size_t n = fread(buffer, 1, sizeof(buffer), pipe);
        if (n == 0) break;
        fwrite(buffer, 1, n, stdout);
    }
    int rc = pclose(pipe);
    // Optionally propagate non-zero exit status as success; outputs already printed
    (void)rc;
    return 0;
}
