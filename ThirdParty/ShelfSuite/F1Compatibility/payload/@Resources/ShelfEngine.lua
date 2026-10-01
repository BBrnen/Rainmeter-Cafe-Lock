--[[
    ShelfEngine.lua — ShelfSuite v2.0
    Motor dinamico que genera tabs e iconos desde una tabla de configuracion.
    Cada Shelf (modulo) tiene su propio config.lua que define tabs e items.

    Autor: Martin Santos
    Licencia: MIT
]]

-- Will be populated by dofile() from config.lua or config.example.lua
ShelfConfig = {}

function Initialize()
    -- Load config: try config.lua first, fallback to config.example.lua
    -- This way the distributable works out-of-the-box with examples,
    -- and users can override by creating their own config.lua
    local skinPath = SKIN:GetVariable("CURRENTPATH")
    local configFile = skinPath .. "config.lua"
    local exampleFile = skinPath .. "config.example.lua"

    local f = io.open(configFile, "r")
    if f then
        f:close()
        dofile(configFile)
    else
        dofile(exampleFile)
    end

    ActiveTab = 1

    -- Read shared variables from Rainmeter
    Columns = tonumber(SKIN:GetVariable("Columns")) or 6
    IconSpacing = tonumber(SKIN:GetVariable("IconSpacing")) or 75
    IconSize = tonumber(SKIN:GetVariable("IconSize")) or 40
    IconRowHeight = tonumber(SKIN:GetVariable("IconRowHeight")) or 70
    PaddingX = tonumber(SKIN:GetVariable("PaddingX")) or 15
    ContentStartY = tonumber(SKIN:GetVariable("ContentStartY")) or 55
    HoverOffset = tonumber(SKIN:GetVariable("HoverOffset")) or 3
    MaxSlots = 18  -- Must match number of MeterIcon slots defined in Shelf.ini

    -- Calculate widget height based on largest tab
    CalculateHeight()

    -- Generate initial content for tab 1
    SwitchTab(1)
end

function CalculateHeight()
    local maxItems = 0
    for _, tab in ipairs(ShelfConfig.tabs) do
        if #tab.items > maxItems then
            maxItems = #tab.items
        end
    end

    local rows = math.ceil(maxItems / Columns)
    -- Height = tab area (50) + rows * IconRowHeight + bottom padding (20)
    local height = 50 + (rows * IconRowHeight) + 20

    -- Minimum height
    if height < 120 then height = 120 end

    SKIN:Bang("!SetVariable", "WidgetHeight", tostring(height))
    SKIN:Bang("!SetOption", "MeterBackground", "Shape",
        "Rectangle 0,0," .. SKIN:GetVariable("WidgetWidth") .. "," .. height .. "," ..
        SKIN:GetVariable("CornerRadius") .. " | Fill Color " ..
        SKIN:GetVariable("MainBackground") .. " | StrokeWidth 0")
end

function SwitchTab(tabIndex)
    ActiveTab = tabIndex

    -- Update tab visuals
    UpdateTabs()

    -- Generate content for active tab
    GenerateContent()

    SKIN:Bang("!UpdateMeter", "*")
    SKIN:Bang("!Redraw")
end

function UpdateTabs()
    local activeColor = SKIN:GetVariable("ActiveTabColor")
    local inactiveColor = SKIN:GetVariable("InactiveTabColor")
    local activeText = SKIN:GetVariable("ActiveTextColor")
    local inactiveText = SKIN:GetVariable("InactiveTextColor")
    local tabWidth = tonumber(SKIN:GetVariable("TabWidth")) or 85
    local tabHorizontalPadding = tonumber(SKIN:GetVariable("TabHorizontalPadding")) or 12
    local widgetBaseWidth = tonumber(SKIN:GetVariable("WidgetBaseWidth")) or 480
    local tabHeight = SKIN:GetVariable("TabHeight")
    local cornerRadius = SKIN:GetVariable("CornerRadius")
    local paddingX = SKIN:GetVariable("PaddingX")
    local paddingY = SKIN:GetVariable("PaddingY")
    local tabSpacing = SKIN:GetVariable("TabSpacing")

    local numTabs = #ShelfConfig.tabs
    local maxTabs = 5  -- Max tab meters defined in INI

    local xPos = tonumber(paddingX) or 0
    -- Show and configure active tabs
    for i, tab in ipairs(ShelfConfig.tabs) do
        if i > maxTabs then break end

        local isActive = (i == ActiveTab)
        local color = isActive and activeColor or inactiveColor
        local textColor = isActive and activeText or inactiveText

        SKIN:Bang("!SetOption", "MeterTab" .. i .. "Text", "Text", tab.name)
        SKIN:Bang("!UpdateMeter", "MeterTab" .. i .. "Text")
        local width = math.max(tabWidth, SKIN:GetMeter("MeterTab" .. i .. "Text"):GetW() + (tabHorizontalPadding * 2))

        -- Update tab background shape and position
        SKIN:Bang("!SetOption", "MeterTab" .. i .. "Bg", "Shape",
            "Rectangle 0,0," .. width .. "," .. tabHeight .. "," ..
            cornerRadius .. "," .. cornerRadius ..
            " | Fill Color " .. color .. " | StrokeWidth 0")
        SKIN:Bang("!SetOption", "MeterTab" .. i .. "Bg", "X", tostring(xPos))
        SKIN:Bang("!SetOption", "MeterTab" .. i .. "Bg", "Y", paddingY)

        -- Update tab text: name, position, color
        SKIN:Bang("!SetOption", "MeterTab" .. i .. "Text", "FontColor", textColor)
        SKIN:Bang("!SetOption", "MeterTab" .. i .. "Text", "X",
            tostring(xPos + (width / 2)))
        SKIN:Bang("!SetOption", "MeterTab" .. i .. "Text", "Y",
            tostring(tonumber(paddingY) + (tonumber(tabHeight) / 2)))

        -- Show tab meters
        SKIN:Bang("!ShowMeter", "MeterTab" .. i .. "Bg")
        SKIN:Bang("!ShowMeter", "MeterTab" .. i .. "Text")
        xPos = xPos + width + (tonumber(tabSpacing) or 0)
    end

    local widgetWidth = math.max(widgetBaseWidth, xPos - (tonumber(tabSpacing) or 0))
    SKIN:Bang("!SetVariable", "WidgetWidth", tostring(widgetWidth))
    SKIN:Bang("!SetOption", "MeterBackground", "Shape",
        "Rectangle 0,0," .. widgetWidth .. "," .. SKIN:GetVariable("WidgetHeight") .. "," ..
        cornerRadius .. " | Fill Color " .. SKIN:GetVariable("MainBackground") .. " | StrokeWidth 0")
    SKIN:Bang("!SetOption", "MeterSettingsGear", "X", tostring(widgetWidth - 28))

    -- Hide unused tab meters
    for i = numTabs + 1, maxTabs do
        SKIN:Bang("!HideMeter", "MeterTab" .. i .. "Bg")
        SKIN:Bang("!HideMeter", "MeterTab" .. i .. "Text")
    end
end

function GenerateContent()
    local tab = ShelfConfig.tabs[ActiveTab]
    if not tab then return end

    local items = tab.items
    local iconPath = SKIN:GetVariable("@") .. "Icons\\"
    local defaultIcon = ShelfConfig.defaultIcon or "folder.png"

    -- Populate visible slots
    for i = 1, math.min(#items, MaxSlots) do
        local item = items[i]
        local label = item.label or item[1] or ""
        local action = item.action or item[2] or ""
        local icon = item.icon or item[3] or defaultIcon

        -- Check if custom icon exists, otherwise use specified or default
        local customIcon = iconPath .. label .. ".png"
        local iconFile = iconPath .. icon

        -- Calculate grid position
        local col = (i - 1) % Columns
        local row = math.floor((i - 1) / Columns)
        local x = PaddingX + 20 + (col * IconSpacing)
        local y = ContentStartY + 10 + (row * IconRowHeight)

        -- Update icon meter
        SKIN:Bang("!SetOption", "MeterIcon" .. i, "ImageName", iconFile)
        SKIN:Bang("!SetOption", "MeterIcon" .. i, "X", tostring(x))
        SKIN:Bang("!SetOption", "MeterIcon" .. i, "Y", tostring(y))
        SKIN:Bang("!SetOption", "MeterIcon" .. i, "LeftMouseUpAction", '[\"' .. action .. '\"]')
        SKIN:Bang("!SetOption", "MeterIcon" .. i, "MouseOverAction",
            "[!SetOption MeterIcon" .. i .. " Y " .. (y - HoverOffset) .. "]" ..
            "[!UpdateMeter MeterIcon" .. i .. "][!UpdateMeter MeterIcon" .. i .. "Text][!Redraw]")
        SKIN:Bang("!SetOption", "MeterIcon" .. i, "MouseLeaveAction",
            "[!SetOption MeterIcon" .. i .. " Y " .. y .. "]" ..
            "[!UpdateMeter MeterIcon" .. i .. "][!UpdateMeter MeterIcon" .. i .. "Text][!Redraw]")
        SKIN:Bang("!SetOption", "MeterIcon" .. i, "ToolTipText", action)
        SKIN:Bang("!ShowMeter", "MeterIcon" .. i)

        -- Update label meter
        SKIN:Bang("!SetOption", "MeterIcon" .. i .. "Text", "Text", label)
        SKIN:Bang("!SetOption", "MeterIcon" .. i .. "Text", "X",
            tostring(x + (IconSize / 2)))
        SKIN:Bang("!SetOption", "MeterIcon" .. i .. "Text", "Y",
            tostring(y + IconSize + 5))
        SKIN:Bang("!ShowMeter", "MeterIcon" .. i .. "Text")
    end

    -- Hide unused slots
    for i = #items + 1, MaxSlots do
        SKIN:Bang("!HideMeter", "MeterIcon" .. i)
        SKIN:Bang("!HideMeter", "MeterIcon" .. i .. "Text")
    end
end

function GetIconPath(label, specifiedIcon)
    local iconPath = SKIN:GetVariable("@") .. "Icons\\"

    -- Priority 1: custom icon named after the label
    -- (user places MyLabel.png in Icons folder)
    -- Priority 2: icon specified in config
    -- Priority 3: default icon

    return iconPath .. (specifiedIcon or ShelfConfig.defaultIcon or "folder.png")
end
