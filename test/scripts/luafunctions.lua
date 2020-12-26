function OnUpdate(table, index)
    return table[index]
end

function Maths(a, b, c)
    return a * b + c
end

function Object(object)
    object.m_prop = 0
    object.Setter(object.m_prop + 1)
end

function VecTest(a, b, c)
    return { a, b, c }
end

function TestCallback()
    return function()
        cppFunction("passed_again")
    end
end

function TestCallbackArgs()
    return function(text)
        cppFunction(text)
    end
end

function GetMap()
    return {
        x = 2,
        y = "something",
        z = false,
        w = {
            r = 3.14,
            g = true
        }
    }
end

function GetMapValue(map)
    return map.w.g
end
