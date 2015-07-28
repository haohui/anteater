# Adopted from http://snippets.dzone.com/posts/show/5124
class UnionSet
  attr_accessor :parent, :rank

  def initialize
    @parent = self
    @rank = 0
  end

  def union(other)
    self_root = self.find
    other_root = other.find
    if self_root.rank > other_root.rank
      other_root.parent = self_root
    elsif self_root.rank < other_root.rank
      self_root.parent = other_root
    elsif self_root != other_root
      other_root.parent = self_root
      self_root.rank += 1
    end
  end

  def find
    if self.parent === self
      self
    else
      self.parent = self.parent.find
    end
  end

end
