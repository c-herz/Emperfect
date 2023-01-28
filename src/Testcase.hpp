/**
 *  @note This file is part of Emperfect, https://github.com/mercere99/Emperfect
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2023.
 *
 *  @file  Testcase.hpp
 *  @brief An individual testcase in Emperfect.
 * 
 *  @todo bold failed test cases in PrintCode
 */

#ifndef EMPERFECT_TESTCASE_HPP
#define EMPERFECT_TESTCASE_HPP

#include "emp/base/vector.hpp"

#include "CheckInfo.hpp"

class Testcase {
  friend class Emperfect;
private:
  // -- Configured from args --
  std::string name;          // Name of this test case.
  size_t id;                 // Unique ID number for this test case.
  double points = 0.0;       // Number of points this test case is worth.

  std::string input_filename;  // Name of file to feed as standard input, if any.
  std::string expect_filename; // Name of file to compare against standard output.
  std::string code_filename;   // Name of file with code to test.
  std::string args;            // Command-line arguments.

  // Names for generated files.
  std::string cpp_filename;     // To create with C++ code for this test
  std::string compile_filename; // To collect compiler outputs (usually warnings and errors)
  std::string exe_filename;     // To create for executable
  std::string output_filename;  // To collect output generated by executable.
  std::string error_filename;   // To collect errors generated by executable.
  std::string result_filename;  // To log results for test checks.

  // Testcase run configs.
  bool call_main = true;     // Should main() function be called?
  bool hidden = false;       // Should this test case be seen by students?
  bool match_case = true;    // Does case need to match perfectly in the output?
  bool match_space = true;   // Does whitespace need to match perfectly in the output?
  size_t timeout = 5;        // How many seconds should this testcase be allowed run?

  // -- Configured elsewhere --
  string_block_t code;       // The actual code associated with this test case.
  std::string processed_code; // Code after ${} are filled in and collapsed to one string.
  std::string filename;      // What file is this test case originally in?
  size_t start_line = 0;     // At which line number is this test case start?
  size_t end_line = 0;       // At which line does this test case end?

  std::vector<CheckInfo> checks;

  // -- Results --
  int compile_exit_code = -1;  // Exit code from compilation (results compiler_filename)
  int run_exit_code = -1;      // Exit code from running the test.
  bool output_match = true;    // Did exe output match expected output?
  bool hit_timeout = false;    // Did this testcase need to be halted?
  double score = 0.0;          // Final score awarded for this testcase.

  // Helper functions

  // Determine how many tests match a particular lambda.
  size_t CountIf(auto test) const {
    return std::count_if(checks.begin(), checks.end(), test);
  }

public:
  Testcase(size_t _id) : id(_id) { }

  size_t GetNumChecks() const { return checks.size(); }
  size_t CountPassed() const {
    return CountIf([](const auto & check){ return check.passed; });
  }
  size_t CountFailed() const {
    return CountIf([](const auto & check){ return !check.passed; });
  }

  bool Passed() const {
    return CountPassed() == checks.size() && output_match && !hit_timeout && !compile_exit_code;
  }

  // Test if a check at particular line number passed.
  bool Passed(size_t test_id) const {
    for (const auto & check : checks) {
      if (check.id == test_id) return check.passed;
    }
    return true;  // No check on line == passed.
  }

  double EarnedPoints() const { return Passed() ? points : 0.0; }

  // Take an input line and convert any "CHECK" macro into full analysis and output code.
  std::string ProcessChecks() {
    std::stringstream out;

    // We need to identify the comparator and divide up arguments in CHECK.
    size_t check_pos = processed_code.find("CHECK(");
    size_t check_end = 0;
    size_t check_id = 0;
    while (check_pos != std::string::npos) {
      std::string location = emp::to_string("Test #", id, ", Check #", check_id);

      // Output everything from the end of the last check to the beginning of this one.
      out << processed_code.substr(check_end, check_pos-check_end);

      // Isolate this check and divide it into arguments.
      check_end = emp::find_paren_match(processed_code, check_pos+5);
      const std::string check_body = processed_code.substr(check_pos+6, check_end-check_pos-6);
      check_end += 2;  // Advance the end past the ");" at the end of the check.

      checks.emplace_back(check_body, location, check_id);
      out << checks.back().ToCPP();

      // Find the next check and loop starting from the end of this one.
      check_id++;
      check_pos = processed_code.find("CHECK(", check_end);
    }

    // Grab the rest of the processed_code and output the processed string.
    out << processed_code.substr(check_end);
    return out.str();
  }
  
  void GenerateCPP(const std::string & header) {
    // If we are using a file for test code, load it in.
    if (code_filename.size()) {
      emp::notify::TestError(code.size(),
        "Test case ", id, " cannot have both a code filename and in-place code provided.");

      emp::File file(code_filename);
      code = file.GetAllLines();
    }

    // Start with boilerplate.
    std::cout << "Creating: " << cpp_filename << std::endl;
    std::ofstream cpp_file(cpp_filename);
    cpp_file
      << "// This is a test file autogenerated by Emperfect.\n"
      << "// See: https://github.com/mercere99/Emperfect\n\n"
      << "#include <fstream>\n"
      << "#include <iostream>\n"
      << "#include <sstream>\n"
      << "\n"
      << header << "\n"
      << "void _emperfect_main() {\n"
      << "  std::ofstream _emperfect_results(\"" << result_filename << "\");\n"
      << "  bool _emperfect_passed = true;\n"
      << "  size_t _emperfect_check_id = 0;\n\n";

    // Add updated code for this specific test.
    cpp_file << ProcessChecks() << "\n";

    // Close out the main and make sure it get's run appropriately.
    cpp_file
      << "  _emperfect_results << \"SCORE \" << (_emperfect_passed ? "
          << points << " : 0) << \"\\n\";\n"
      << "}\n\n"
      << "/* Build a test runner to be executed before main(). */\n"
      << "struct _emperfect_runner {\n"
      << "  _emperfect_runner() {\n"
      << "    _emperfect_main();\n";
    if (call_main == false) cpp_file << "    exit(0); // Don't execute main().\n";
    cpp_file
      << "  }\n"
      << "};\n\n"
      << "static _emperfect_runner runner;\n";
  }


  void PrintCode(OutputInfo & output) const {
    std::ostream & out = output.GetFile();

    if (output.IsHTML()) {
      out << "Source:<br><br>\n";
      out << "<table style=\"background-color:#E3E0CF;\"><tr><td><pre>\n\n";
      for (auto line : code) {
        const bool highlight = false; // Passed(0);
        if (highlight) out << "<b>";
        out << line << "\n";
        if (highlight) out << "</b>";
      }
      out << "</pre></tr></table>\n";
    } else {
      out << "Source:\n\n";
      for (auto line : code) out << line << "\n";
    }
  }

  void PrintResult_Title(OutputInfo & output) const {
    std::ostream & out = output.GetFile();

    if (output.IsHTML()) {
      out << "<h2>Test Case " << id << ": " << name;
      if (hidden) out << " <small>[HIDDEN]</small>";
      out << "</h2>\n";
    } else {
      out << "TEST CASE " << id << ": " << name;
      if (hidden) out << " [HIDDEN]";
      out << "\n";
    }
  }

  void PrintResult_Success(OutputInfo & output) const {
    std::ostream & out = output.GetFile();

    // Notify whether the overall test passed.
    std::string color = "green";
    std::string message = "PASSED!";
    if (!Passed()) {
      color = "red";
      if (compile_exit_code) message = "FAILED during compilation.";
      else if (hit_timeout) message = "FAILED due to timeout.";
      else if (!output_match) message = "FAILED due to mis-matched output.";
      else message = "FAILED due to unsuccessful check.";
    }

    if (output.IsHTML()) {
      out << "<b>Result: <span style=\"color: " << color << "\">"
          << message << "</b></span><br><br>\n\n";
    } else {
      out << "Result: " << message << "\n";
    }
  }

  void PrintResult_Checks(OutputInfo & output) const {
    // Print the results for each check.
    if (!hidden || output.HasHiddenDetails()) {
      for (const auto & check : checks) {
        check.PrintResults(output);
      }
    }
  }

  void PrintResult(OutputInfo & output) const {
    if (!output.HasResults()) return;

    PrintResult_Title(output);
    PrintResult_Success(output);
    PrintResult_Checks(output);

    bool print_code = (!hidden || output.HasHiddenDetails()) &&
                      (!Passed() || output.HasPassedDetails());
    if (print_code) PrintCode(output);
  }

  void PrintDebug(std::ostream & out=std::cout) {
    out << "===============\n"
        << "Name..............: " << name << "\n"
        << "Points............: " << points << "\n"
        << "Hidden............: " << (hidden ? "true" : "false") << "\n"
        << "match_case........: " << (match_case ? "true" : "false") << "\n"
        << "match_space.......: " << (match_space ? "true" : "false") << "\n"
        << "call_main.........: " << (call_main ? "true" : "false") << "\n"
        << "Command Line Args.: " << args << "\n"
        << "FILENAME Input to provide...: " << (input_filename.size() ? input_filename : "(none)") << "\n"
        << "FILENAME Expected output....: " << (expect_filename.size() ? expect_filename : "(none)") << "\n"
        << "FILENAME Code for testcase..: " << (code_filename.size() ? code_filename : "(none)") << "\n"
        << "FILENAME Generated CPP file.: " << (cpp_filename.size() ? cpp_filename : "(none)") << "\n"
        << "FILENAME Compiler results...: " << (compile_filename.size() ? compile_filename : "(none)") << "\n"
        << "FILENAME Executable.........: " << (exe_filename.size() ? exe_filename : "(none)") << "\n"
        << "FILENAME Execution output...: " << (output_filename.size() ? output_filename : "(none)") << "\n"
        << "FILENAME Execution errors...: " << (error_filename.size() ? error_filename : "(none)") << "\n"
        << "FILENAME Log of test results: " << (result_filename.size() ? result_filename : "(none)") << "\n"
        << std::endl;

    // std::string code_filename; // Name of file with code to test.
    // string_block_t code;       // The actual code associated with this test case.
    // std::string filename;      // What file is this test case in?
    // size_t start_line = 0;     // At which line number is this test case start?
    // size_t end_line = 0;       // At which line does this test case end?
    // std::vector<CheckInfo> checks;
  }
};

#endif
