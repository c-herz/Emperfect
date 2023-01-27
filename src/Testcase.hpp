/**
 *  @note This file is part of Emperfect, https://github.com/mercere99/Emperfect
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2023.
 *
 *  @file  Testcase.hpp
 *  @brief An individual testcase in Emperfect.
 * 
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

  // Generated files.
  std::string cpp_filename;     // To create with C++ code for this test
  std::string compile_filename; // To collect compiler outputs.
  std::string exe_filename;     // To create for executable
  std::string output_filename;  // To collect output generated by executable.
  std::string error_filename;   // to collect errors generated by executable.

  bool call_main = true;     // Should main() function be called?
  bool hidden = false;       // Should this test case be seen by students?
  bool match_case = true;    // Does case need to match perfectly in the output?
  bool match_space = true;   // Does whitespace need to match perfectly in the output?

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
  bool output_match = false;   // Did exe output match expected output?

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

  bool Passed() const { return CountPassed() == checks.size(); }

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
  
  void GenerateCPP(const std::string & header,
                   const std::string & dir,
                   const std::string & log_filename) {
    // If we are using a filename, load it in as code.
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
      << "  std::ofstream _emperfect_out(\"" << dir << "/" << log_filename << "\", std::ios_base::app);\n"
      << "  _emperfect_out << \"TESTCASE " << id << "\\n\";\n"
      << "  bool _emperfect_passed = true;\n"
      << "  size_t _emperfect_check_id = 0;\n\n";

    // Add updated code for this specific test.
    cpp_file << ProcessChecks() << "\n";

    // Close out the main and make sure it get's run appropriately.
    cpp_file
      << "  _emperfect_out << \"SCORE \" << (_emperfect_passed ? "
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


  void PrintCode_HTML(std::ostream & out) const {
    out << "<table style=\"background-color:#E3E0CF;\"><tr><td><pre>\n\n";
    std::ifstream source(filename);
    std::string code;
    size_t line = 0;

    // Skip beginning of file.
    while (++line < start_line) std::getline(source, code);
    while (++line < end_line && std::getline(source, code)) {
      const bool highlight = !Passed(line-1);
      if (highlight) out << "<b>";
      out << code << "\n";
      if (highlight) out << "</b>";
    }
    out << "</pre></tr></table>\n";
  }

  void PrintResult_HTML(std::ostream & out, bool is_student=true) const {
    // Notify if the test passed.
    if (Passed()) {
      out << "Test case <span style=\"color: green\"><b>Passed!</b></span><br><br>\n\n";
    }
    else {
      // Failed cases will already have an error noted except for hidden cases in student file.
      if (hidden && is_student) {
        out << "Test case <span style=\"color: red\"><b>Failed.</b></span><br><br>\n";
        return; // No more details.
      }
    
      out << "Source";
      if (!is_student) out << " (starting from line " << start_line << ")";
      out << ":<br><br>\n";

      PrintCode_HTML(out);
    }
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
        << "Input to provide..: " << (input_filename.size() ? input_filename : "(none)") << "\n"
        << "Expected output...: " << (expect_filename.size() ? expect_filename : "(none)") << "\n"
        << "Generated output..: " << (output_filename.size() ? output_filename : "(none)") << "\n"
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
