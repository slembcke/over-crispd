require 'set'

def jumble()
	arr = [
		"A", "B", "C", "D",
		"A", "B", "C", "D",
		"A", "B", "C", "D",
	].sort_by{rand}
	
	return [
		arr[0x0] + arr[0x6],
		arr[0x1] + arr[0x7],
		arr[0x2] + arr[0x8],
		arr[0x3] + arr[0x9],
		arr[0x4] + arr[0xA],
		arr[0x5] + arr[0xB],
	]
end

foo = []
bar = []

16.times do
	arr1 = jumble()
	set1 = Set.new(arr1)

	loop do
		arr2 = jumble()
		if set1.intersection(arr2).size == 0 then
			foo << "\t{" + arr1.collect{|str| "GENE_#{str[0]}#{str[1]}"}.join(", ") + "},"
			bar << "\t{" + arr2.collect{|str| "GENE_#{str[0]}#{str[1]}"}.join(", ") + "},"
			break
		end
	end
end

puts foo
puts
puts bar
