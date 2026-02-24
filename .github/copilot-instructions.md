Do not build the project.
Using "auto" type is strictly prohibited.
Follow SOLID class design principles to the best of your ability:
1. A class should have a single, clearly defined responsibility.
2. Internal data should be encapsulated and accessed through controlled interfaces.
3. All members of a class should contribute to a cohesive purpose.
4. Each method should perform one task without introducing side effects.
5. Constructors should initialize state, not execute complex operations.
6. The public interface should be stable, predictable, and easy to understand.
7. Dependencies should be minimized and kept loosely coupled.
8. Avoid overly large, multi-purpose classes that centralize too much functionality.
9. Prefer composition to inheritance when structuring functionality.
10. Class behavior should be consistent and free of unexpected actions.

Do not hesitate to create a new class when a larger amount of already written code would suggest it.
Never keep legacy code, even for backwards compatibility. Always replace it with new changes.
Write code for Qt6.8 only. 
If an issue seems difficult to pinpoint add more LOG() statements and request user to provide log output.
