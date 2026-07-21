# Minecraft Forge Mod Generator

lil can generate complete Minecraft Forge mods from a high-level DSL. Use `-j` to enable Java generation and `-o <dir>/` to write a full mod directory tree.

## Quick Start

```lil
[MCMForge]
include minecraft-forge

modID@minecraft = "example_mod"
modVersion@minecraft = "0.1.0"
mcVersion@minecraft = "1.20.1"
modName@minecraft = "Example Mod"
modAuthors@minecraft = "you"
```

Compile: `lil -j mod.lil -o mod_out/`

## Metadata

| Directive | Description |
|-----------|-------------|
| `modID@minecraft` | Mod identifier (used in file paths, registry names) |
| `modVersion@minecraft` | Mod version |
| `mcVersion@minecraft` | Minecraft version target |
| `modName@minecraft` | Human-readable mod name |
| `modAuthors@minecraft` | Author string |

## Items

### Basic Items

```lil
newItem@minecraft(ruby) {
    propertiesItem@minecraft {
        stacksTo = 64
    }
    itemNames@minecraft {
        [1] = "Ruby"
    }
}
```

### Tools (automatic type detection)

Names containing `_sword`, `_pickaxe`, `_axe`, `_shovel`, or `_hoe` are automatically typed:

```lil
newItem@minecraft(ruby_sword, ruby_pickaxe, ruby_axe) {
    propertiesItem@minecraft {
        tier = "ruby"
        attackDamage = 3
        attackSpeed = -2.4
    }
    itemNames@minecraft {
        [1] = "Ruby Sword"
        [2] = "Ruby Pickaxe"
        [3] = "Ruby Axe"
    }
}
```

### Armor

Armor names (`_helmet`, `_chestplate`, `_leggings`, `_boots`) are detected:

```lil
newItem@minecraft(ruby_helmet, ruby_boots) {
    propertiesItem@minecraft {
        tier = "ruby"
        durability = 37
        protection = 3, 6, 8, 3
        toughness = 2.0
    }
    itemNames@minecraft {
        [1] = "Ruby Helmet"
        [2] = "Ruby Boots"
    }
}
```

### Food

```lil
newFood@minecraft(ruby_apple) {
    propertiesItem@minecraft {
        hunger = 6
        saturation = 0.6
        alwaysEdible = true
    }
    foodEffect@minecraft("NIGHT_VISION", 6000, 0, 1.0)
    foodEffect@minecraft("REGENERATION", 100, 1, 1.0)
}
```

### Properties

| Property | Applies To | Type | Description |
|----------|-----------|------|-------------|
| `tier` | tools, armor | string | Tool tier (e.g. "ruby", "diamond") |
| `attackDamage` | tools | number | Base attack damage |
| `attackSpeed` | tools | number | Attack speed modifier |
| `durability` | any | number | Item durability |
| `stacksTo` | items | number | Max stack size |
| `isImmuneToLava` | any | 0/1 | Fire resistance |
| `hunger` | food | number | Hunger restored |
| `saturation` | food | number | Saturation modifier |
| `alwaysEdible` | food | 0/1 | Edible when full |
| `protection` | armor | list | Per-slot protection values |
| `toughness` | armor | number | Armor toughness |

## Blocks

```lil
newBlock@minecraft(ruby_block, ruby_ore) {
    propertiesBlock@minecraft {
        material = "metal"
        hardness = 5.0
        resistance = 6.0
        tool = "pickaxe"
        tier = "iron"
        sound = "metal"
    }
    itemNames@minecraft {
        [1] = "Ruby Block"
        [2] = "Ruby Ore"
    }
}
```

### Block Properties

| Property | Values | Description |
|----------|--------|-------------|
| `material` | `metal`, `wood`, `stone`, `glass`, `dirt`, `plant`, `snow` | Base material |
| `hardness` | number | Mining hardness |
| `resistance` | number | Explosion resistance |
| `tool` | `pickaxe`, `axe`, `shovel`, `hoe` | Required tool type |
| `tier` | `stone`, `iron`, `diamond` | Required tool tier |
| `sound` | `metal`, `wood`, `stone`, `glass`, `deepslate`, `gravel`, `snow` | Block sound type |
| `requiresTool` | 0/1 | Tool required for drops |
| `light` | number (0-15) | Light emission level |

## Recipes

### Shaped

```lil
recipeShaped@minecraft(ruby_sword) {
    pattern {
        "R", "R",
        "R", "S"
    }
    keys = "R": "ruby", "S": "stick"
    result = "ruby_sword"
}
```

### Shapeless

```lil
recipeShapeless@minecraft(ruby) {
    ingredients = "ruby_block"
    result = "ruby"
    count = 9
}
```

### Smelting

```lil
recipeSmelting@minecraft(ruby) {
    ingredient = "raw_ruby"
    result = "ruby"
    experience = 1.0
    cookTime = 200
}
```

## Creative Tabs

```lil
newCreativeTab@minecraft(ruby_tab) {
    icon = "ruby_sword"
    items = "ruby", "ruby_sword", "ruby_block", "ruby_apple"
}
```

Multiple tabs are supported:

```lil
newCreativeTab@minecraft(materials_tab) {
    icon = "ruby"
    items = "ruby", "ruby_block"
}
newCreativeTab@minecraft(combat_tab) {
    icon = "ruby_sword"
    items = "ruby_sword", "ruby_helmet"
}
```

## Procedure Events

Items and blocks can have custom behavior via procedure blocks:

### Item Events

```lil
procedureItem@minecraft(HitEntity) {
    write@std "hit something"
}
```

| Event Name | Override Method |
|------------|----------------|
| `RightClick` | `InteractionResultHolder<ItemStack> use(Level, Player, InteractionHand)` |
| `HitEntity` | `boolean hurtEnemy(ItemStack, LivingEntity, LivingEntity)` |
| `Crafted` | `void onCraftedBy(ItemStack, Level, Player)` |

### Block Events

```lil
procedureBlock@minecraft(BlockPlaced) {
    write@std "placed"
}
```

| Event Name | Override Method |
|------------|----------------|
| `BlockPlaced` | `void onPlace(BlockState, Level, BlockPos, BlockState, boolean)` |

## Generated Files

When using `-o <dir>/`, lil produces:

```
<dir>/
  build.gradle
  gradle.properties
  src/main/java/com/{modid}/
    ModMain.java
    ModItems.java
    ModBlocks.java
    ModCreativeTabs.java
    ModArmorMaterials.java
    ModToolTiers.java
    {ClassName}.java           (one per item/block)
  src/main/resources/
    META-INF/mods.toml
    pack.mcmeta
    assets/{modid}/
      lang/en_us.json
      models/item/{name}.json
      models/block/{name}.json
      blockstates/{name}.json
    data/{modid}/
      recipes/{name}.json
      loot_tables/blocks/{name}.json
      tags/blocks/mineable/{tool}.json
      tags/blocks/needs_{tier}_tool.json
```
