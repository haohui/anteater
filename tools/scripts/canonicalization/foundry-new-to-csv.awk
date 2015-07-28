function ltrim(s) { sub(/^[ \t]+/, "", s); return s }
function rtrim(s) { sub(/[ \t]+$/, "", s); return s }
function trim(s)  { return rtrim(ltrim(s)); }

{ printf "%s,%s,%s,%s\n", trim(substr($0,8,20)), trim(substr($0,28,16)), trim(substr($0,44,12)), trim(substr($0,70,5)); }
