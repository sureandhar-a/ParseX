- Context: docs/lld.md's Parser data model pseudocode shows Cluster : DomainObjectBase (public inheritance).

- Decision: implemented as composition instead — each domain type holds a CommonFields common; member.

- Reasoning: no virtual dispatch needed anywhere in the design; avoids slicing; follows C++ Core Guidelines' composition-over-inheritance guidance for pure-data relationships.

- Consequences: field access is object.common.shortName instead of object.shortName — slightly more verbose, but the type safety and guideline-alignment are worth it. If runtime polymorphism is ever needed later, this decision would need revisiting.