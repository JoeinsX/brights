#pragma once

struct SystemContext;

struct System {
   virtual ~System() = default;
   virtual void run(SystemContext& ctx) = 0;
};
