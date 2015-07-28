def parallel_map(worker_num, list, &block)
  return nil if !list
  return [] if list.empty?

  result = Array.new(list.length)
  queue = []
  list.each_index {|i| queue << [i, list[i]] }
  mutex = Mutex.new
  threads = Array.new(worker_num) do
    Thread.new do
      while true
        payload = nil
        mutex.synchronize { payload = queue.slice!(0) }
        Thread.current.kill if !payload
        r = block.call(payload[1])
        mutex.synchronize { result[payload[0]] = r }
      end
    end
  end
  threads.each {|thr| thr.join }
  return result
end

def parallel_each(worker_num, list, &block)
  return if !list or list.empty?
  queue = [].concat(list)
  mutex = Mutex.new
  threads = Array.new(worker_num) do
    Thread.new do
      while true
        payload = nil
        mutex.synchronize { payload = queue.slice!(0) }
        Thread.current.kill if !payload
        block.call(payload)
      end
    end
  end
  threads.each {|thr| thr.join }  
end
