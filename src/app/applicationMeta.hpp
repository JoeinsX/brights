#pragma once

template<typename T, typename Settings>
concept ApplicationComponent = requires(T t, Settings s) { t.initAppComponent(s); };

template<typename Settings, ApplicationComponent<Settings>... T>
class AppComponentsMixin: public Settings,
                          public T... {
protected:
   explicit AppComponentsMixin(Settings& settings): T{}... { (..., T::initAppComponent(settings)); }

   template<typename AppComponent>
   AppComponent& getComponent() {
      return static_cast<AppComponent&>(*this);
   }

   template<typename AppComponent>
   [[nodiscard]] const AppComponent& getComponent() const {
      return static_cast<const AppComponent&>(*this);
   }
};
