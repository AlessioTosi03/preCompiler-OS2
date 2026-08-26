# C Precompiler - OS2 Project

A C99-compliant precompiler utility designed to parse source code files, validate variable identifiers and types, track variable usages, and output a detailed analysis report.

---

## Features

- Standard & Custom Type Support: Validates standard primitive types (including composite type modifiers like unsigned long long) and custom types registered via typedef.
- Identifier Validation: Verifies variable names against C naming conventions and reserved keywords.
- Pointer Handling: Correctly strips pointer dereference operators (*) to validate underlying variable identifiers.
- Literal & Comment Stripping: Masks string/character literals and removes single-line (//) or multi-line (/* */) comments to avoid false-positive variable tracking.
- Detailed Error Reporting: Generates statistics on total variables checked, invalid variable names, unknown types, and unused variables.

---

## Compilation

Compile the project using gcc with standard C99 flags:

```bash
gcc -Wall -Wextra -std=c99 main.c precompiler.c -o myPreCompiler
Usage
Run the compiled executable with the input source file:

Bash
./myPreCompiler [options] <input_file.c>
Options
-v, --verbose: Displays processing statistics and error report directly in standard output (stdout).

-o <output_file>, --output <output_file>: Writes the generated report into a specified text file.

##Example Commands
Print report directly to the terminal:

Bash
./myPreCompiler -v test_4.c


##Output Structure
The program outputs processing statistics followed by a detailed list of detected errors:

=== MYPRECOMPILER PROCESSING STATISTICS ===
Analyzed file: test_4.c
Total variables checked: 3
Total errors detected: 1
 - Invalid variable names: 0
 - Invalid data types: 1
 - Unused variables: 0

--- DETECTED ERRORS LIST ---
[Line 2] Error: Type not valid: BadType
