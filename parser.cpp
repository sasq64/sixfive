#include "parser.h"

#include <boost/spirit.hpp>
#include <boost/spirit/iterator/position_iterator.hpp>

#include <unordered_map>
#include <vector>
#include <functional>
#include <stack>
#include <string>
#include <cstring>
#include <memory>
#include <boost/spirit/error_handling/exceptions.hpp>

using namespace boost::spirit;

namespace sixfive {

struct AsmState
{
	bool isMonitor = false;
	std::unordered_map<std::string, double> symbols;
	std::vector<std::string> undefined;
	uint32_t orgStart = 0x1000;
	uint32_t org;

	std::string moncmd;
	std::vector<int> monargs;
	std::string monstring;

};

typedef position_iterator<char const*> iterator_t;

static auto symbol_p = []() -> auto { return ch_p('*') | ch_p('$') | lexeme_d[ (ch_p('_') | alpha_p) >> *(ch_p('_') | alnum_p) ]; } ;

struct AsmGrammar : public boost::spirit::grammar<AsmGrammar>
{
	std::function<int(uint16_t org, const std::string &op, const std::string &arg)> encode;

	AsmGrammar(AsmState &as) : state(as) {}

	AsmState& state;


	template <typename ScannerT> struct definition
	{
		using Rule = boost::spirit::rule<ScannerT>;

		std::unordered_map<std::string, double> &symbols;
		std::vector<std::string> &undefined;

		definition(AsmGrammar const &g) : grammar(g), state(g.state), symbols(g.state.symbols), undefined(g.state.undefined) {
			symbols["PI"] = M_PI;
			s0_ap = constant_ap[fpushnum] | ('(' >> s6_ap >> ')');
		}

		using FnN = std::function<void(int)>;
		using FnC = std::function<void(char)>;
		using FnD = std::function<void(double)>;
		using Fn = std::function<void(iterator_t, iterator_t)>; //const char *, const char *)>;

		std::unordered_map<std::string, std::function<double(double, double)>> ops = {
			{ "+", [](double a, double b) -> double { return a + b; } },
			{ "-", [](double a, double b) -> double { return a - b; } },
			{ "*", [](double a, double b) -> double { return a * b; } },
			{ "/", [](double a, double b) -> double { return a / b; } },
			{ "%", [](double a, double b) -> double { return (unsigned)a % (unsigned)b; } },
			{ "&", [](double a, double b) -> double { return (unsigned)a & (unsigned)b; } },
			{ "|", [](double a, double b) -> double { return (unsigned)a | (unsigned)b; } },
			{ "&&", [](double a, double b) -> double { return (unsigned)a && (unsigned)b; } },
			{ "||", [](double a, double b) -> double { return (unsigned)a || (unsigned)b; } },
			{ ">>", [](double a, double b) -> double { return (unsigned)a >> (unsigned)b; } },
			{ "<<", [](double a, double b) -> double { return (unsigned)a << (unsigned)b; } }
		};

		const AsmGrammar& grammar;

		AsmState &state;

		std::stack<double> valstack;
		std::string opcodeName;
		std::string labelName;
		std::string symbolName;
		std::string argExp;
		std::string opcodeArg;
		double expValue;
		struct
		{
			bool negate = false;
			double val;
		} e;

		FnC fnegate = [=](const char) { e.negate = true; };
		FnD fsetnum = [=](double n) { e.val = n; };

		Fn fpushnum = [=](auto, auto) {
			valstack.push(e.negate ? -e.val : e.val);
			e.negate = false;
			e.val = 1234.5678;
		};

		Fn fop = [=](auto a, auto b) { 
			static const char* xchars = "<>/*+-=%|&^!~";
			b = a;
			while(strchr(xchars, *b)) b++;
			auto op = std::string(a, b);
			//auto op = opName;
			auto v1 = valstack.top();
			valstack.pop();
			auto v0 = valstack.top();
			valstack.pop();
			valstack.push(ops[op](v0, v1));
		};

		Fn fexpression = [=](auto, auto) {
			expValue = valstack.top();
			valstack.pop();
		};

		Fn fsym = [=](auto a, auto b) {
			std::string s(a, b);
			//printf("SYM '%s'\n", s.c_str());
			if(s == "*" || s == "$") {
				e.val = state.org;
				return;
			}

			auto it = std::find_if(symbols.begin(), symbols.end(), [&](auto p) -> auto {
					if(s.length() != p.first.length())
						return false;
					for(int i=0; i<(int)s.length(); i++)
						if(toupper(p.first[i]) != toupper(s[i]))
							return false;
						return true;
					});
			//auto it = symbols.find(s);
			if(it == symbols.end())
				undefined.push_back(s);
			else
				e.val = it->second;
		};

		Fn flabel = [=](auto a, auto b) {
			while(b[-1] == ':') b--;
			std::string label(a, b);
			printf("LABEL %s\n", label.c_str());
			symbols[label] = state.org;
		};


		Fn fopcode = [=](auto a, auto b) {
			opcodeName = std::string(a,b);
			opcodeArg = "";
		};


		Fn foparg = [=](auto a, auto b) {
			std::string arg(a, b);

			auto pos = arg.find(argExp);
			arg.replace(pos, argExp.length(), std::to_string((long long)expValue));
			auto e = std::remove(arg.begin(), arg.end(), ' ');
			*e = 0;
			std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
			opcodeArg = arg;
		};

		Fn fasmline = [=](auto b, auto e) {
			std::transform(opcodeName.begin(), opcodeName.end(), opcodeName.begin(), ::tolower);
			int len = grammar.encode(state.org, opcodeName, opcodeArg);
			if(len < 0)
				throw_(b, std::string("Error"));
			else
				state.org += len;
		};

		Fn fmetaline = [=](auto b, auto) {
			int len = grammar.encode(state.org, symbolName, argExp);
			if(len < 0)
				throw_(b, std::string("Error"));
			else
				state.org += len;
		};

		Fn femptyline = [=](auto, auto) {
		};

		Fn fdefline = [=](auto, auto) {
			if(symbolName == "*" || symbolName == "$") {
				state.org = expValue;
				return;
			}

			symbols[symbolName] = expValue;
			printf("Assign '%f' to '%s'\n", expValue, symbolName.c_str());
		};

		std::vector<uint8_t> data;

		Fn fdataline = [=](auto b, auto) {
			if(data.size() > 0) {
				//printf("DATA\n");
				int len = grammar.encode(state.org, "b", std::string((const char*)&data[0], data.size())); 
				if(len < 0)
					throw_(b, std::string("Error"));
				else
					state.org += len;
				data.clear();
			}
		};

		Fn fpushdata = [=](auto a, auto b) {
			data.push_back(expValue);
			//printf("DATA %02x\n", (uint8_t)expValue);
		};

		Fn fstrdata = [=](auto a, auto b) {
			grammar.encode(state.org, "b", std::string((const char*)&data[0], data.size())); 
		};

		bool foundSol;

		Fn fsol = [=](auto a, auto b) {
			//printf("%c %02x\n", *a, a[-1]);
			foundSol = (a[-1] == 0xa);
		};

		std::function<bool(void)> fwassol = [=]() {
			return foundSol;
		};

		Fn SetMe(std::string &target) {
			return [&](iterator_t a, iterator_t b) {
				target = std::string(a, b);
			};
		};

		Fn Log(const std::string &text) {
			return [&](iterator_t a, iterator_t b) {
				printf("%s (%s)\n", text.c_str(), std::string(a, b).c_str());
			};
		};

		Fn fmodasm = [=](auto a, auto b) {
			std::transform(opcodeName.begin(), opcodeName.end(), opcodeName.begin(), ::tolower);
			state.monstring = opcodeName + " " + opcodeArg;
		};
	//	Rule symbol_ap = ch_p('*') | ch_p('$') | lexeme_d[ (ch_p('_') | alpha_p) >> *(ch_p('_') | alnum_p) ] ;

		Rule constant_ap =
		    lexeme_d[!ch_p('-')[fnegate] >>
		             (((str_p("0x") | ch_p('$')) >> hex_p[fsetnum]) |
		              (ch_p('%') >> bin_p[fsetnum]) | real_p[fsetnum] |
		              ( (ch_p('$') | (  (ch_p('_') | alpha_p) >> *(ch_p('_') | alnum_p) ) )[fsym] | uint_p[fsetnum]) |
		              (ch_p('\'') >> anychar_p[fsetnum] >> ch_p('\'')) |
		              (ch_p('\"') >> anychar_p[fsetnum] >> ch_p('\"')) )
			];

		// Moved to constructor
		//Rule s0_ap = constant_ap[fpushnum] | ('(' >> s6_ap >> ')');
		Rule s0_ap;

		Rule s1_ap = s0_ap >> *(('*' >> s0_ap)[fop] | ('/' >> s0_ap)[fop] |
		                        ('%' >> s0_ap)[fop]);

		Rule s2_ap = s1_ap >> *(('+' >> s1_ap)[fop] | ('-' >> s1_ap)[fop]);

		Rule s3_ap = s2_ap >> *((">>" >> s2_ap)[fop] | ("<<" >> s2_ap)[fop]);

		Rule s4_ap = s3_ap >> *(('>' >> s3_ap)[fop] | ('<' >> s3_ap)[fop] |
		                        ("==" >> s3_ap)[fop] | (">=" >> s3_ap)[fop] |
		                        ("<=" >> s3_ap)[fop] | ("!=" >> s3_ap)[fop]);

		Rule s5_ap = s4_ap >> *(('|' >> s4_ap)[fop] | ('&' >> s4_ap)[fop] |
		                        ('^' >> s4_ap)[fop]);

		Rule s6_ap = s5_ap >> *(("||" >> s5_ap)[fop] | ("&&" >> s5_ap)[fop]);

		Rule expression_ap = s6_ap[fexpression];

		Rule opcode_ap = lexeme_d[ as_lower_d[ alpha_p >> *('_' | alnum_p) ] ];
		Rule reg_p = as_lower_d[ ch_p('x') | ch_p('y') ];

		Rule oparg_ap = ('(' >> expression_ap[SetMe(argExp)] >> !(',' >> reg_p) >> ')'>>  !(',' >> reg_p)) |
		   	(!ch_p('#') >> expression_ap[SetMe(argExp)] >> !(',' >> reg_p)) ;

		Rule asmcode_ap = opcode_ap[fopcode] >> !oparg_ap[foparg] ;

		Rule label_ap = lexeme_d[ (ch_p('.') | ch_p('_') | alpha_p)[fsol] >> *(ch_p('_') | alnum_p) >> (ch_p(':') | eps_p(fwassol)) ];

		Rule asmline_ap = !label_ap[flabel] >> asmcode_ap >> (comment_p(";") | eol_p);
		Rule emptyline_ap = !label_ap[flabel] >> (comment_p(";") | eol_p);

		Rule defline_ap = symbol_p()[SetMe(symbolName)] >> '=' >> expression_ap >> (comment_p(";") | eol_p);

		Rule metaline_ap = lexeme_d[ ch_p('@')  >> symbol_p() ][SetMe(symbolName)] >> lexeme_d[ *print_p ][SetMe(argExp)] >> (comment_p(";") | eol_p);

		Rule dataline_ap = !label_ap[flabel] >>
		                   as_lower_d[str_p("db") | str_p(".byte")] >>
		                   (  confix_p('"', (*c_escape_ch_p)[fstrdata], '"') |
		                      (expression_ap[fpushdata] >>
		                      *("," >> expression_ap[fpushdata]))  ) >>
		                   (comment_p(";") | eol_p);

		Rule mainrule_ap = *(emptyline_ap[femptyline] | dataline_ap[fdataline] | asmline_ap[fasmline] | defline_ap[fdefline] | metaline_ap[fmetaline] );

		Rule adr_ap = hex_p[push_back_a(state.monargs)];
		Rule monasmline_ap = ch_p('a')[assign_a(state.moncmd)] >> !adr_ap >> asmcode_ap;
		Rule monarg_p = hex_p[push_back_a(state.monargs)];
		Rule monstrarg_p = lexeme_d[ alpha_p >> *alnum_p ];
		Rule monname_ap = lexeme_d[ alpha_p >> *alnum_p ];
		Rule moncmd_ap = monname_ap[assign_a(state.moncmd)] >> ( monstrarg_p[assign_a(state.monstring)] | *monarg_p ); 
		Rule monline_ap = monasmline_ap[fmodasm] | defline_ap[Log("DEF")] | moncmd_ap; 

		Rule const &start() const {
			if(state.isMonitor)
				return monline_ap;
			else
				return mainrule_ap; 
		}
	};
};

struct MonParser::Impl {
	AsmState state;
	AsmGrammar g;
	Impl() : g(state) {
		g.state.isMonitor = true;
		g.encode = [](uint16_t org, const std::string &op, const std::string &arg) -> int {
			printf("OP %s at %x\n", op.c_str(), org);
			return 1;
		};
	}

	Command parseLine(const std::string &line) {
		auto code = line; // (std::string("\n") + line + "\n");
		iterator_t begin(code.c_str(), code.c_str() + code.length());
		iterator_t end;
		state.moncmd = state.monstring = "";
		state.monargs.clear();
		auto result = boost::spirit::parse(begin, end, g, blank_p);
		auto fp = result.stop.get_position();
		Command c;
		c.valid = false;
		if(result.full) {
			c.valid = true;
			c.name = state.moncmd;
			c.args = state.monargs;
			c.strarg = state.monstring;
		}
		return c;

	}

};

MonParser::~MonParser() = default;
MonParser::MonParser(MonParser &&op) noexcept = default;
MonParser &MonParser::operator=(MonParser &&op) noexcept = default;

MonParser::MonParser() : impl(std::make_unique<MonParser::Impl>()) {
};

MonParser::Command MonParser::parseLine(const std::string &line) {
	return impl->parseLine(line);
};

bool parse(const std::string &code, 
		std::function<int(uint16_t org, const std::string &op, const std::string &arg)> encode) {

	AsmState state;

	AsmGrammar g(state);
	g.encode = encode;
	int ucount = -1;
	while(true) {
		state.org = state.orgStart;
		auto code2 = (std::string("\n") + code + "\n");

		iterator_t begin(code.c_str(), code.c_str()+code.length());
		iterator_t end;

		file_position fp;
		fp.file = "dummy.asm";
		begin.set_position(fp);

		try {
			auto result = boost::spirit::parse(begin, end, g, blank_p);
			fp = result.stop.get_position();
			if(!result.full)
				return false;
		} catch(parser_error<std::string, iterator_t> e) {
			auto pos = e.where.get_position();
			printf("Parse error in %s %d (%d)\n", pos.file.c_str(), pos.line, pos.column);
			return false;
		}

		if(state.undefined.size() > 0) {
				for(const auto &ud : state.undefined)
					printf("'%s' is undefined\n", ud.c_str());
			if(ucount >= 0 && (int)state.undefined.size() >= ucount) {
				printf("Error\n");
				return false;
			}
			ucount = state.undefined.size();
			state.undefined.clear();
			

		} else
			return true;

	}
	//return result.full;
}

} // namespace
