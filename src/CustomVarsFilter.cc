// +------------------------------------------------------------------+
// |             ____ _               _        __  __ _  __           |
// |            / ___| |__   ___  ___| | __   |  \/  | |/ /           |
// |           | |   | '_ \ / _ \/ __| |/ /   | |\/| | ' /            |
// |           | |___| | | |  __/ (__|   <    | |  | | . \            |
// |            \____|_| |_|\___|\___|_|\_\___|_|  |_|_|\_\           |
// |                                                                  |
// | Copyright Mathias Kettner 2012             mk@mathias-kettner.de |
// +------------------------------------------------------------------+
//
// This file is part of Check_MK.
// The official homepage is at http://mathias-kettner.de/check_mk.
//
// check_mk is free software;  you can redistribute it and/or modify it
// under the  terms of the  GNU General Public License  as published by
// the Free Software Foundation in version 2.  check_mk is  distributed
// in the hope that it will be useful, but WITHOUT ANY WARRANTY;  with-
// out even the implied warranty of  MERCHANTABILITY  or  FITNESS FOR A
// PARTICULAR PURPOSE. See the  GNU General Public License for more de-
// ails.  You should have  received  a copy of the  GNU  General Public
// License along with GNU Make; see the file  COPYING.  If  not,  write
// to the Free Software Foundation, Inc., 51 Franklin St,  Fifth Floor,
// Boston, MA 02110-1301 USA.

#include "CustomVarsFilter.h"
#include "opids.h"
#include "logger.h"
#include <ctype.h>
#include "OutputBuffer.h"
#include <strings.h>
#include <string.h>

	CustomVarsFilter::CustomVarsFilter(CustomVarsColumn *column, int opid, char *value)
    : _column(column)
    , _opid(abs(opid))
    , _negate(opid < 0)
    , _ref_text(value)
    , _regex_matcher(0)
{
    // Prepare part in case of DICT filter
    if (_column->type() == COLTYPE_DICT) {
        /* Filter for custom_variables:
Filter: custom_variables = PATH /hirni.mk

The variable name is part of the value and separated
with spaces
         */
        const char *cstr = _ref_text.c_str();
        const char *search_space = cstr;
        UErrorCode status = U_ZERO_ERROR;
        while (*search_space && !isspace(*search_space))
            search_space ++;
        _ref_varname = std::string(cstr, search_space - cstr);
        while (*search_space && isspace(*search_space))
            search_space ++;
        _ref_string = search_space;

        // Prepare regular expression
        if (_opid == OP_REGEX || _opid == OP_REGEX_ICASE) {
            if (strchr(search_space, '{') || strchr(search_space, '}')) {
                setError(RESPONSE_CODE_INVALID_HEADER, "disallowed regular expression '%s': must not contain { or }", value);
            }
            else {
                UnicodeString s = UnicodeString::fromUTF8(search_space);
                _regex_matcher = new RegexMatcher(s, (_opid == OP_REGEX_ICASE ? UREGEX_CASE_INSENSITIVE : 0), status);
                if (U_FAILURE(status))
                {
                    setError(RESPONSE_CODE_INVALID_HEADER, "invalid regular expression '%s'", value);
                    delete _regex_matcher;
                    _regex_matcher = 0;
                }
            }
        }
    }
}

CustomVarsFilter::~CustomVarsFilter()
{
    if (_regex_matcher) {
        delete _regex_matcher;
    }
}

bool CustomVarsFilter::accepts(void *data)
{
    if (_column->type() == COLTYPE_DICT)
    {
        const char *act_string = _column->getVariable(data, _ref_varname.c_str());
        if (!act_string)
            act_string = "";

        bool pass = true;
        switch (_opid) {
            case OP_EQUAL:
                pass = _ref_string == act_string; break;
            case OP_EQUAL_ICASE:
                pass = !strcasecmp(_ref_string.c_str(), act_string); break;
            case OP_REGEX:
            case OP_REGEX_ICASE:
                if ( _regex_matcher != 0) {
                    UnicodeString s = UnicodeString::fromUTF8(act_string);
                    _regex_matcher->reset(s);
                    pass = _regex_matcher->find();
                }
                else {
                    pass = false;
                }
                break;
            case OP_GREATER:
                pass = 0 > strcmp(_ref_string.c_str(), act_string); break;
            case OP_LESS:
                pass = 0 < strcmp(_ref_string.c_str(), act_string); break;
            default:
                // this should never be reached, all operators are handled
                logger(LG_INFO, "Sorry. Operator %d for strings not implemented.", _opid);
                break;
        }
        return pass != _negate;

    }
    else
    {
        bool is_member = _column->contains(data, _ref_text.c_str());
        switch (_opid) {
            case OP_LESS:
                return (!is_member) == (!_negate);
            default:
                logger(LG_INFO, "Sorry, Operator %s for custom variable lists not implemented.", op_names_plus_8[_opid]);
                return true;
        }
    }
}



