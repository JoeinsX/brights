#pragma once

template<typename Lambda, int = (Lambda{}(), 0)>
consteval bool isConstantExpressionLambda(Lambda /*lambda*/) {
   return true;
}
consteval bool isConstantExpressionLambda(...) {
   return false;
}

#define ASSERT_EXPRESSION_CONSTEVALABILITY(expr) static_assert(isConstantExpressionLambda([]() { expr; }));
