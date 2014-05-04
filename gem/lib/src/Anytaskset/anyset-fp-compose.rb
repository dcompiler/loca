#!/usr/bin/ruby
#
# This file reads a sharing graph file, a
# sfp profile file and thread group. It 
# composes the concurrent sfp of that 
# thread subset
#
# example run:
#
# anyset-fp-compose.rb -m [max_threads] -l [lowest_pillar] -p [pillar_step] -f [sfp_file] -g [sharing_graph.sg] --group=0,1,2,4
#
# e.g. ./anyset-fp-compose.rb -m 8 -l 10 -p 1 -f fp.out -g sg.out --group=1,2,3,4
#
# Written by Hao Luo
#
#

require 'optparse'

options = {}

OptionParser.new do |opts|
  opts.on('-m', '--max-threads [MAX_THREAD]', Integer, "The Max Threads") do |f|
    options[:max_thread] = f
  end
  opts.on('-l', '--lowest-pillar [LOWEST_PILLAR]', Integer, "The lowest pillar") do |f|
    options[:lowest_pillar] = f
  end
  opts.on('-p', '--pillar-step [PILLAR_STEP]', Integer, "The pillar step") do |f|
    options[:pillar_step] = f
  end
  opts.on('-f', '--sfp-profile [FILE]', String, "The sfp profile file") do |f|
    options[:sfp_profile] = f
  end
  opts.on('-g', '--sharing-graph [FILE]', String, "The sharing graph file") do |f|
    options[:sharing_graph] = f
  end
  opts.on('--group x,y,z', Array, "The thread group") do |f|
    options[:thread_group] = f.map {|x| x.to_i }
  end
end.parse!

#p options
#p ARGV

class SFPCurve

  attr_accessor :ft
  attr_accessor :sfp
  attr_accessor :length

  def initialize(n)
    @ft = Array.new
    @sfp = Array.new
    @length = n
  end

  def [](i)
    @sfp[i]
  end

  def ft_at(i)
    @ft[i]
  end

end

class SFPCurveGroup

  attr_accessor :curve_count
  attr_accessor :impl

  def initialize(n)
    @curve_count = n
    @impl = Array.new(n)
  end

  def set_curve(c, sharing_degree)
    puts "[ERROR] error trying to add a non-SFPCurve into SFPCurveGroup" if not c.is_a?(SFPCurve)
    @impl[sharing_degree-1] = c
  end

  def at(sharing_degree, index)
    @impl[sharing_degree-1][index]
  end

  def ft_at(index)
    @impl[0].ft_at(index)
  end

  def [](index)
    @impl[index-1]
  end

end

class Composer

  attr_accessor :max_threads, :sgraph, :sfp
  attr_reader :weight_map
  attr_accessor :curve_length
  attr_accessor :gLowestPillar, :pillar_step, :gPillarLengths

  def initialize(max_thread, sgraph_file, sfp_file, lowest_pillar, pillar_step)
    @max_threads = max_thread
    @weight_map = Array.new
    @gPillarLengths = Array.new
    @gLowestPillar = lowest_pillar
    @pillar_step = pillar_step

    read_sfp_profile(sfp_file)
    read_sgraph(sgraph_file)
  end

  # ========================================
  # read the sgraph from file
  # ========================================
  def read_sgraph(filename)

    @sgraph = Array.new

    #  FIXME max trace length 2^35 ?
    #
    (@gLowestPillar..35).each do |ext|
      sg_filename = filename + ".#{ext}"
      if not File.exist?( sg_filename )
        return
      end
      temp = Hash.new
      File.open(sg_filename,"r").each_line do |l|
        temp[l.split("\t")[0].reverse.to_i(2)] = l.split("\t")[1].to_f
      end
      @sgraph << temp
      @gPillarLengths << (1<<(ext<<@pillar_step))
    end

  end

  # =========================================
  # read the sfp profile from file and populate
  # the related attributes
  # =========================================
  def read_sfp_profile(filename)

    @sfp = SFPCurveGroup.new(@max_threads)

    num_lines = 0
    curves = Array.new(@max_threads)

    # read the total lines of the file
    File.open(filename,"r").each_line do |l|
      num_lines += 1
      if num_lines <= 2 # first two lines are the header
        next
      end
    end

    @curve_length = num_lines - 2

    # initialize new SFPCurves
    (0..@max_threads-1).each do |i|
      curves[i] = SFPCurve.new(@curve_length)
    end

    num_lines = 0
    File.open(filename,"r").each_line do |l|

      num_lines += 1
      if num_lines <= 2 # first two lines are the header
        next
      end

      splitted_line = l.split("\t")
      for i in (0..@max_threads-1)
        curves[i].ft << splitted_line[0].to_i
        curves[i].sfp << (splitted_line[i+1].to_f - splitted_line[i+2].to_f)
      end

    end

    # add curves to CurveGroup
    (1..@max_threads).each do |i|
      @sfp.set_curve(curves[i-1], i)
    end
  
  end

  # =========================================
  # count the '1's in the bitmap
  # =========================================
  def self.count_ones(n)

    n.to_s(2).count("1")

  end

  
  # =========================================
  # compute the total weights for all thread
  # count and cache it in weight_map 
  # =========================================
  def cache_total_weights

    @sgraph.each do |h|

      temp_map = Hash.new

      h.keys.each do |x|
        n = Composer.count_ones(x)
        if not temp_map.has_key?(n)
          temp_map[n] = h[x]
        else
          temp_map[n] += h[x]
        end
      end

      @weight_map << temp_map

    end

  end

  # ==========================================
  # translate a bitmap(integer) into an array
  # ==========================================
  def self.bitmap_to_array(n)

    ret = Array.new
    k = 1
    while n!=0 do
      ret << k if n%2==1
      k += 1
      n >>= 1
    end
    return ret

  end

  # ==========================================
  # translate an array into a bitmap(integer)
  # ==========================================
  def self.array_to_bitmap(a)

    a.inject(0) { |n, i| n += (1<<(i-1)) }

  end

  # ==========================================
  # test if b1 is b2's subset
  # TODO do we allow b1 is b2's superset?
  # ==========================================
  def self.is_subset_or_superset(b1, b2)

    (b1&b2) == b1

  end

  # ==========================================
  # test if b1 and b2 have intersetion
  # ==========================================
  def self.has_intersection?(b1, b2)

    (b1&b2) != 0

  end

  # ==========================================
  # get portion of sfp for a subset of threads
  # the subset of threads is represented as 
  # bitmap, e.g. integer
  # ==========================================
  def sfp_partial(thread_group)

    porp = Array.new
    (0..@sgraph.size-1).each do |i|
      porp << Array.new(@max_threads+1, 0)
    end
   
    (0..@sgraph.size-1).each do |j|

      @sgraph[j].keys.each do |g|
        i = Composer.count_ones(g)
        if Composer.has_intersection?(thread_group, g) and @weight_map[j][i] != 0
          porp[j][i] += @sgraph[j][g].to_f / @weight_map[j][i].to_f
        end
      end

    end

    ret = SFPCurve.new(@curve_length)
    phase = 0
    max_ft_at_current_phase = @gPillarLengths[phase]

    (0..@curve_length-1).each do |i|
      ws = @sfp.ft_at(i)
      ret.ft << ws
      if ws > max_ft_at_current_phase
        phase = phase + 1
        max_ft_at_current_phase = @gPillarLengths[phase]
      end 

      # current window smaller than max_ft at this phase
      ret.sfp << (1..@max_threads).to_a.inject(0) { |r, k|
        if phase == 0 
          r += porp[phase][k] * @sfp[k][i]
        elsif phase == @sgraph.size
          r += porp[phase-1][k] * @sfp[k][i]
        else
          c = porp[phase-1][k] + ((ws - @gPillarLengths[phase-1]).to_f / (@gPillarLengths[phase] - @gPillarLengths[phase-1]).to_f * (porp[phase][k]-porp[phase-1][k]))
          r += c * @sfp[k][i]
        end
        r
      }

    end
    return ret

  end

  # ==========================================
  # Debugging helpers
  # ==========================================

  def dump_hash(h)

    h.each_key do |k|
      puts "#{k.to_s(2)} with #{Composer.count_ones(k)} '1's => #{(h[k]*64).to_s}"
    end

  end

  def dump_sfp(profile)
  
    (0..@curve_length-1).each do |e|
      print "#{@sfp.ft_at(e)} :\t"
      (1..@max_threads).each do |f|
        print "#{@sfp[f][e]}\t"
      end
      puts ""
    end 

  end

end

c = Composer.new(options[:max_thread], 
                 options[:sharing_graph], 
                 options[:sfp_profile],
                 options[:lowest_pillar],
                 options[:pillar_step])
c.cache_total_weights
group_bitmap = Composer.array_to_bitmap(options[:thread_group])
puts c.sfp_partial(group_bitmap).sfp
