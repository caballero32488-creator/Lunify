-- Lunify example: Luau types, methods, and dead code.
-- Build a config table and greet players by name.

export type Config = {
    name: string,
    version: number,
}

local function buildConfig(): Config
    local cfg: Config = {
        name = "lunify",
        version = 0.2,
    }
    return cfg
end

local function buildMessage(player: string): string
    local msg = "hello, " .. player
    return msg .. "!"
end

local function main()
    local cfg = buildConfig()
    local players = { "Alice", "Bob" }
    for _, p in ipairs(players) do
        print(cfg.name, buildMessage(p))
    end
end

main()
