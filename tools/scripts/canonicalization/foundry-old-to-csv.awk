function ltrim(s) { sub(/^[ \t]+/, "", s); return s }
function rtrim(s) { sub(/[ \t]+$/, "", s); return s }
function trim(s)  { return rtrim(ltrim(s)); }

{ printf "%s,%s,%s,%s\n", trim(substr($0,8,19)), trim(substr($0,28,16)), trim(substr($0,44,11)), trim(substr($0,62,5)); }
