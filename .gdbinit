define sym
p print_symbol($arg0)
end

define obj
p print_object(heap, $arg0)
end

define detail
p print_detail(heap, $arg0)
end

define stack
p print_stack($arg0)
end

define sl8bt
p print_backtrace($arg0)
end

define sl8usage
p print_objects($arg0)
end
