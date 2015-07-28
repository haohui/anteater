function ltrim(s) { sub(/^[ \t]+/, "", s); return s }
function rtrim(s) { sub(/[ \t]+$/, "", s); return s }
function trim(s)  { return rtrim(ltrim(s)); }

BEGIN {FIELDWIDTHS = "8 16 15 16 11 7 4"};
{ printf "%s,%s,%s,%s,%s\n", trim($2), trim($3), trim($4), trim($5), trim($7); }
