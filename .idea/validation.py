from dataclasses import dataclass
from enum import Enum

class ASTNode:
    pass

class ValidationException(Exception):
    pass

@dataclass
class Trait:
    class FunctionSignature:
        pass

@dataclass
class ClassDeclaration:
    implements: list[Trait] = []
    class FunctionDeclaration:
        pass

    class FieldDeclaration:
        pass

    class Constructor:
        pass

    # Other class members, like methods, fields, constructors, etc.
    pass

@dataclass
class Type:
    pass
@dataclass
class BuiltinType(Type):
    class Kind(Enum):
        I32 = 1
        I64 = 2
        F32 = 3
        F64 = 4
        STR = 5
        BOOL = 6
        ERROR = 7
        # Note: MASSIVE oversimplification, but it works for now

    kind: Kind

class RuntimeObject:
    type: Type
    is_lvalue: bool
    is_mutable: bool

@dataclass
class UserDefinedType(Type):
    decl: ClassDeclaration
    generic_args: list[Type]

class CallableType(Type):
    return_type: Type
    param_types: list[Type]
    generic_params: list[Type]


@dataclass
class Module:
    imports: list[str] = []
    exports: list[str] = []
    ast: ASTNode = None
    pass

@dataclass
class ExportTable:
    pass


class GlobalContext:
    registry_: dict[str, ExportTable] = {}


class ModuleHandler:
    @staticmethod
    def get_standard_library() -> ExportTable:
        return ExportTable()

class FileValidator:
    @staticmethod
    def validate_module(module: Module, global_context: GlobalContext) -> bool:
        validator = FileValidator(global_context)
        
        validator.get_imports(module.imports) # Fetches imports from the global context and adds them to the local context

        validator.validate_module(module.ast) # Validates the module's AST, populating the local context with declarations and checking for errors

        validator.validate_module_exports(module.exports) # Validates that the module's exports are valid, checking against the local context and ensuring that exported items exist and are correctly typed.
        # If everthing is valid, we set the module's export table in the global context, so that other modules can import from it.

        
        return not validator.has_errors()
        
    def evaluate_expression(self, expr: ASTNode) -> Type:
        # Example: if expr is an integer literal
        return BuiltinType(BuiltinType.Kind.I32)
    
    def check_type_compatibility(self, type1: Type, type2: Type) -> bool:
        # Example: if both types are the same builtin type
        if isinstance(type1, BuiltinType) and isinstance(type2, BuiltinType):
            return type1.kind == type2.kind
        # More complex type compatibility checks would go here
        return False
    
    def check_variable_decl(self, node: ASTNode):
        pass

    def check_function_decl(self, node: ASTNode):
        pass

    def check_class_decl(self, node: ASTNode):
        pass

    def check_trait_decl(self, node: ASTNode):
        pass
    

def validate_modules(ordered_modules: list[Module]) -> bool:

    success: bool = True

    global_context = GlobalContext()

    for module in ordered_modules:
        try:
            if not FileValidator.validate_module(module, global_context): # This really shouldnt fail, but just in case. What is should do is use the <error> builtin type to represent any errors that occur during validation, so that we can continue validating the rest of the module and report all errors at once, instead of stopping at the first error.
                success = False
        except ValidationException as e:
            # print(f"Error validating module: {e}")
            success = False

    
            
