/*
 * Copyright (c) 2013 Aaron Moss
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "egg.hpp"
#include "parser.hpp"
#include "visitors/compiler.hpp"
#include "visitors/normalizer.hpp"
#include "visitors/printer.hpp"

/** Egg version */
static const char* VERSION = "0.3.2";

/** Egg usage string */
static const char* USAGE = 
"[-c print|compile] [-i input_file] [-o output_file]\n\
 [--dbg] [--no-norm] [--no-memo] [--quiet] [--help] [--version] [--usage]";

/** Full Egg help string */
static const char* HELP = 
"egg [command] [flags] [input-file [output-file]]\n\
\n\
Supported flags are\n\
 -i --input    input file (default stdin)\n\
 -o --output   output file (default stdout)\n\
 -c --command  command - either compile, print, help, usage, or version \n\
               (default compile)\n\
 -n --name     grammar name - if none given, takes the longest prefix of\n\
               the input or output file name (output preferred) which is a\n\
               valid Egg identifier (default empty)\n\
 -q --quiet    suppress warning output\n\
 --dbg         turn on debugging\n\
 --no-norm     turns off grammar normalization\n\
 --no-memo     turns of grammar memoization\n\
 --usage       print usage message\n\
 --help        print full help message\n\
 --version     print version string\n";

/// Command to run
enum egg_mode {
	PRINT_MODE,    ///< Print grammar
	COMPILE_MODE,  ///< Compile grammar
	USAGE_MODE,    ///< Print usage
	HELP_MODE,     ///< Print help
	VERSION_MODE   ///< Print version
};

/// Type of output file
enum file_type {
	STREAM_TYPE,  ///< Output stream (unknown filetype)
	CPP_HEADER,   ///< C++ header file
	CPP_SOURCE,   ///< C++ source file
	UNKNOWN_TYPE  ///< Unable to determine
};

class args {
private:
	bool eq(const char* lit, char* arg) {
		return std::string(lit) == std::string(arg);
	}

	bool match(const char* shrt, const char* lng, char* arg) {
		std::string a(arg);
		return std::string(shrt) == a || std::string(lng) == a;
	}

	std::string id_prefix(char* s) {
		int len = 0;
		char c = s[len];
		if ( ( c >= 'A' && c <= 'Z' )
				|| ( c >= 'a' && c <= 'z' )
				|| c == '_' ) {
			c = s[++len];
		} else {
			return std::string("");
		}

		while ( ( c >= 'A' && c <= 'Z' )
				|| ( c >= 'a' && c <= 'z' )
				|| ( c >= '0' && c <= '9' )
				|| c == '_' ) {
			c = s[++len];
		}

		return std::string(s, len);
	}
	
	file_type suffix_type(char* s) {
		std::string t(s);
		std::string::size_type dot = t.rfind('.');
		
		if ( dot == std::string::npos ) return UNKNOWN_TYPE;
		
		std::string ext(t, dot+1);
		for (std::string::size_type i = 0; i < ext.size(); ++i) { ext[i] = std::tolower(ext[i]); }
		
		if ( ext == "hpp" || ext == "hxx" || ext == "hh" || ext == "h" ) return CPP_HEADER;
		else if ( ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "c" ) return CPP_SOURCE;
		
		return UNKNOWN_TYPE;
	}

	bool parse_mode(char* s) {
		if ( eq("print", s) ) {
			eMode = PRINT_MODE;
			return true;
		} else if ( eq("compile", s) ) {
			eMode = COMPILE_MODE;
			return true;
		} else if ( eq("help", s) ) {
			eMode = HELP_MODE;
			return true;
		} else if ( eq("usage", s) ) {
			eMode = USAGE_MODE;
			return true;
		} else if ( eq("version", s) ) {
			eMode = VERSION_MODE;
			return true;
		} else {
			return false;
		}
	}

	void parse_input(char* s) {
		in = new std::ifstream(s);
		inName = s;
		if ( !nameFlag && out == nullptr ) {
			pName = id_prefix(s);
		}
	}

	void parse_output(char* s) {
		out = new std::ofstream(s);
		outName = s;
		outType = suffix_type(s);
		if ( !nameFlag ) {
			pName = id_prefix(s);
		}
	}
	
	void parse_name(char* s) {
		pName = id_prefix(s);
		nameFlag = true;
	}

public:
	args(int argc, char** argv) 
		: in(nullptr), out(nullptr), 
		  inName(), outName(), outType(STREAM_TYPE), pName(), 
		  dbgFlag(false), nameFlag(false), normFlag(true), memoFlag(true), quietFlag(false),
		  eMode(COMPILE_MODE) {
		
		i = 1;
		if ( argc <= 1 ) return;

		//parse optional sub-command
		if ( parse_mode(argv[i]) ) { ++i; }
		
		//parse explicit flags
		for (; i < argc; ++i) {
			if ( match("-i", "--input", argv[i]) ) {
				if ( i+1 >= argc ) return;
				parse_input(argv[++i]);
			} else if ( match("-o", "--output", argv[i]) ) {
				if ( i+1 >= argc ) return;
				parse_output(argv[++i]);
			} else if ( match("-c", "--command", argv[i]) ) {
				if ( i+1 >= argc ) return;
				parse_mode(argv[++i]);
			} else if ( match("-n", "--name", argv[i]) ) {
				if ( i+1 >= argc ) return;
				parse_name(argv[++i]);
			} else if ( eq("--dbg", argv[i]) ) {
				dbgFlag = true;
			} else if ( eq("--no-norm", argv[i]) ) {
				normFlag = false;
			} else if ( eq("--no-memo", argv[i]) ) {
				memoFlag = false;
			} else if ( match("-i", "--quiet", argv[i]) ) {
				quietFlag = true;
			} else if ( eq("--usage", argv[i]) ) {
				eMode = USAGE_MODE;
			} else if ( eq("--help", argv[i]) ) {
				eMode = HELP_MODE;
			} else if ( eq("--version", argv[i]) ) {
				eMode = VERSION_MODE;
			} else break;
		}

		//parse optional input, source, and output files
		if ( i < argc && in == nullptr )  parse_input(argv[i++]);
		if ( i < argc && out == nullptr ) parse_output(argv[i++]);
	}

	~args() {
		if ( in != nullptr ) { in->close(); delete in; }
		if ( out != nullptr ) { out->close(); delete out; }
	}

	std::istream& input() { if ( in ) return *in; else return std::cin; }
	std::ostream& output() { if ( out ) return *out; else return std::cout; }
	std::string inputFile() { return in ? inName : "<STDIN>"; }
	std::string outputFile() { return out ? outName : "<STDOUT>"; }
	file_type outputType() { return outType; }
	std::string name() { return pName; }
	bool dbg()  { return dbgFlag; }
	bool norm() { return normFlag; }
	bool memo() { return memoFlag; }
	bool quiet() { return quietFlag; }
	egg_mode mode() { return eMode; }

private:
	int i;				  ///< next unparsed value
	std::ifstream* in;	  ///< pointer to input stream (0 for stdin)
	std::ofstream* out;	  ///< pointer to output stream (0 for stdout)
	std::string inName;   ///< Name of the input file (empty if none)
	std::string outName;  ///< Name of the output file (empty if none)
	file_type outType;    ///< Type of output type (default STREAM_TYPE)
	std::string pName; 	  ///< the name of the parser (empty if none)
	bool dbgFlag;         ///< should egg print debugging information?
	bool nameFlag;		  ///< has the parser name been explicitly set?
	bool normFlag;        ///< should egg do grammar normalization?
	bool memoFlag;        ///< should the generated grammar do memoization?
	bool quietFlag;       ///< should warnings be suppressed?
	egg_mode eMode;		  ///< compiler mode to use
};

/** Command line interface
 *  egg [command] [flags] [input-file [output-file]]
 */
int main(int argc, char** argv) {

	args a(argc, argv);
	
	switch ( a.mode() ) {
	case USAGE_MODE:
		std::cout << argv[0] << " " << USAGE << std::endl;
		return 0;
	case HELP_MODE:
		std::cout << HELP << std::endl;
		return 0;
	case VERSION_MODE:
		std::cout << "Egg version " << VERSION << std::endl;
		return 0;
	default: break;
	}
	
	parser::state ps(a.input());
	ast::grammar_ptr g;
	
	if ( egg::grammar(ps, g) ) {
		if ( a.dbg() ) { std::cout << "DONE PARSING" << std::endl; }
		if ( a.norm() ) {
			visitor::normalizer n;
			n.normalize(*g);
		}

		switch ( a.mode() ) {
		case PRINT_MODE: {      // Pretty-print grammar
			visitor::printer p(a.output());
			p.print(*g);
			break;
		} case COMPILE_MODE: {  // Compile grammar
			visitor::compiler c(a.name(), a.output(), (a.outputType() != CPP_SOURCE));
			c.memo(a.memo());
			auto warnings = c.compile(*g);
			if ( ! a.quiet() ) for ( auto&& warning : warnings ) {
				std::cerr << "WARNING: " << warning << std::endl;
			}
			break;
		} default: break;
		}
		
	} else {
		const parser::error& err = ps.error();
			
		std::cerr << "PARSE FAILURE @" << err.pos.line() << ":" << err.pos.col() << std::endl;
		for (auto msg : err.messages) {
			std::cerr << "\t" << msg << std::endl;
		}
		for (auto exp : err.expected) {
			std::cerr << "\tExpected " << exp << std::endl;
		}
	}

	return 0;
} /* main() */

