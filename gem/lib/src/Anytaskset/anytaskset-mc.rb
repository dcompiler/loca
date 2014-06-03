#!/usr/bin/ruby
#
# This file reads a composed sfp profile of a thread group
# (produced by anytaskset-sfp-compose.rb)
# and a per-thread memory access frequency file to compute
# the miss count of the thread group for a given cache size
#
# example run:
#
# anytaskset-mc.rb -c [cache_size] -f [composed_sfp_file] -p [per_thread_access_frequency] --group=1,2,4
#
# e.g. ./anytaskset-mc.rb -c 0x800000 -f sfp.out -p sfp.out.pt --group=1,2,4
#
# Written by Hao Luo
#
#

require 'optparse'

options = {}

OptionParser.new do |opts|

  opts.on('-c', '--cache-size [CACHE_SIZE]', Integer, "The Shared Cache Size") do |f|
    options[:cache_size] = f
  end

  opts.on('-f', '--sfp-file [FILE]', String, "The composed memory shared footprint file") do |f|
    options[:sfp_file] = f
  end

  opts.on('-p', '--pt-file [FILE]', String, "The per-thread memory access frequency file") do |f|
    options[:pt_file] = f
  end

  opts.on('--group x,y,z', Array, "The thread group") do |f|
    options[:thread_group] = f.map {|x| x.to_i }
  end

end.parse!

class MissCountPredictor

  attr_reader :mem_insts
  attr_reader :fp
  attr_reader :CACHELINE_SIZE

  def initialize(fp_file, pt_file)

    @mem_insts = Array.new
    @fp = Array.new
    @CACHELINE_SIZE = 64

    File.open(pt_file,"r") do |f|
      @mem_insts = f.readline.split("\t").map {|x| x.to_i}
    end
    
    File.open(fp_file, "r").each_line do |l|
      ws = l.split("\t")[0].to_f
      fp = l.split("\t")[1].to_f
      @fp << [ws, fp]
    end

  end

  def predict(cache_size, thread_group)

    total_mem_accesses = 0
    thread_group.each do |i|
      total_mem_accesses += @mem_insts[i-1]
    end
  
    mr = (@fp[-1][1] - @fp[-2][1]) / (@CACHELINE_SIZE * (@fp[-1][0] - @fp[-2][0]))

    (0..@fp.length-2).each do |i|
      if @fp[i+1][1] > cache_size and @fp[i][1] <= cache_size
        mr = (@fp[i+1][1] - @fp[i][1]) / ( @CACHELINE_SIZE * (@fp[i+1][0] - @fp[i][0]))
      end
    end

    puts "Miss ratio: #{mr*100}%"
    puts "Mem access: #{total_mem_accesses}"

    return mr * total_mem_accesses

  end

end

# read 
# File.open(filename,"r").each_line do |l|
puts MissCountPredictor.new(options[:sfp_file], options[:pt_file]).predict(options[:cache_size], options[:thread_group])
