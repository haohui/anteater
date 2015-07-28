# FFI bindings for anteater

require 'ffi'

module Anteater
  module C
    extend ::FFI::Library
    ffi_lib ['libSPACoreShared']

    # Symbolic Packet
    attach_function :AnteaterSymbolicPacketCreate, [], :pointer
    attach_function :AnteaterSymbolicPacketFree, [:pointer], :void
    attach_function :AnteaterSymbolicPacketGet, [:pointer, :int, :pointer], :pointer
    attach_function :AnteaterSymbolicPacketInstantiateToIR, [:pointer, :pointer], :void
    attach_function :AnteaterSymbolicPacketAppendField, [:int], :void
    attach_function :AnteaterSymbolicPacketClearFields, [], :void

    # IRBuilder
    attach_function :AnteaterIRBuilderCreate, [], :pointer
    attach_function :AnteaterIRBuilderFree, [:pointer], :void
    attach_function :AnteaterIRBuilderCreateMainFunction, [:pointer], :void
    attach_function :AnteaterIRBuilderClearMainFunction, [:pointer], :void
    attach_function :AnteaterIRBuilderClearConstraintFunctions, [:pointer], :void
    attach_function :AnteaterIRBuilderCreateAnd, [:pointer, :pointer, :pointer], :pointer
    attach_function :AnteaterIRBuilderCreateOr, [:pointer, :pointer, :pointer], :pointer
    attach_function :AnteaterIRBuilderCreateEQ, [:pointer, :pointer, :pointer], :pointer
    attach_function :AnteaterIRBuilderCreateXor, [:pointer, :pointer, :pointer], :pointer
    attach_function :AnteaterIRBuilderCreateAssertion, [:pointer, :pointer], :pointer
    attach_function :AnteaterIRBuilderCreateNot, [:pointer, :pointer], :pointer
    attach_function :AnteaterIRBuilderCreateLoad, [:pointer, :pointer], :pointer
    attach_function :AnteaterIRBuilderCreateEdgeFunction, [:pointer, :pointer, :pointer], :pointer
    attach_function :AnteaterIRBuilderCreateCallEdgeFunction, [:pointer, :pointer, :pointer, :pointer], :pointer
    attach_function :AnteaterIRBuilderCreateCallFilterFunction, [:pointer], :pointer
    attach_function :AnteaterIRBuilderCreateRet, [:pointer, :pointer], :pointer
    attach_function :AnteaterIRBuilderCheckPointMainFunction, [:pointer], :void
    attach_function :AnteaterIRBuilderRewindToCheckPoint, [:pointer], :void
    attach_function :AnteaterIRBuilderGetConstantInt, [:pointer, :long, :int], :pointer
    attach_function :AnteaterIRBuilderGetConstantBitVector, [:pointer, :long, :int], :pointer
    attach_function :AnteaterIRBuilderGetConstantBool, [:pointer, :int], :pointer
    attach_function :AnteaterIRBuilderGetIdenticalTransformationFunction, [:pointer], :pointer
    attach_function :AnteaterIRBuilderDumpModule, [:pointer, :string], :int

  end

  class SymbolicPacket < FFI::AutoPointer
    private_class_method :new
    def initialize(ptr)
      super(ptr)
      @ptr = ptr
    end

    def self.release(ptr)
      C.AnteaterSymbolicPacketFree(ptr)
    end
    
    def self.create
      new(C.AnteaterSymbolicPacketCreate)
    end
    
    def get(idx, builder)
      C.AnteaterSymbolicPacketGet(@ptr, idx, builder)
    end
    
    def instantiate_to_ir(builder)
      C.AnteaterSymbolicPacketInstantiateToIR(@ptr, builder)
    end
    
    def self.set_fields(list)
      C.AnteaterSymbolicPacketClearFields
      list.each {|l| C.AnteaterSymbolicPacketAppendField(l) }
    end
    def to_ptr
      @ptr
    end
  end

  class IRBuilder
    private_class_method :new
    def initialize(ptr)
      @ptr = ptr
    end
    def finalize
      C.AnteaterIRBuilderFree(@ptr)
    end
    def to_ptr
      @ptr
    end

    def self.create
      new(C.AnteaterIRBuilderCreate)
    end
    def create_main_function
      C.AnteaterIRBuilderCreateMainFunction(@ptr)
    end
    def clear_main_function
      C.AnteaterIRBuilderClearMainFunction(@ptr)
    end
    def clear_constraint_functions
      C.AnteaterIRBuilderClearConstraintFunctions(@ptr)
    end
    def create_and(lhs, rhs)
      C.AnteaterIRBuilderCreateAnd(@ptr, lhs, rhs)
    end
    def create_or(lhs, rhs)
      C.AnteaterIRBuilderCreateOr(@ptr, lhs, rhs)
    end
    def create_eq(lhs, rhs)
      C.AnteaterIRBuilderCreateEQ(@ptr, lhs, rhs)
    end
    def create_xor(lhs, rhs)
      C.AnteaterIRBuilderCreateXor(@ptr, lhs, rhs)
    end
    def assert(v)
      C.AnteaterIRBuilderCreateAssertion(@ptr, v)
    end
    def create_not(v)
      C.AnteaterIRBuilderCreateNot(@ptr, v)
    end
    def create_load(v)
      C.AnteaterIRBuilderCreateLoad(@ptr, v)
    end
    def create_edge_function
      pkt_in, pkt_out = SymbolicPacket.create, SymbolicPacket.create
      func = C.AnteaterIRBuilderCreateEdgeFunction(@ptr, pkt_in.to_ptr, pkt_out.to_ptr)
      return [pkt_in, pkt_out, func]
    end
    def create_call_edge_function(f, pkt_in, pkt_out)
      C.AnteaterIRBuilderCreateCallEdgeFunction(@ptr, f, pkt_in.to_ptr, pkt_out.to_ptr)
    end
    def create_call_filter_function
      C.AnteaterIRBuilderCreateCallFilterFunction(@ptr)
    end
    def create_ret(v)
      C.AnteaterIRBuilderCreateRet(@ptr, v)
    end
    def checkpoint_main_function
      C.AnteaterIRBuilderCheckPointMainFunction(@ptr)
    end
    def rewind_to_checkpoint
      C.AnteaterIRBuilderRewindToCheckPoint(@ptr)
    end
    def constant_int(v, len)
      C.AnteaterIRBuilderGetConstantInt(@ptr, v, len)
    end
    def constant_bitvector(v, len)
      C.AnteaterIRBuilderGetConstantBitVector(@ptr, v, len)
    end
    def true
      C.AnteaterIRBuilderGetConstantBool(@ptr, 1)
    end
    def false
      C.AnteaterIRBuilderGetConstantBool(@ptr, 0)
    end
    def identical_transformation
      C.AnteaterIRBuilderGetIdenticalTransformationFunction(@ptr)
    end
    def dump(filename)
      C.AnteaterIRBuilderDumpModule(@ptr, filename)
    end
  end
end
